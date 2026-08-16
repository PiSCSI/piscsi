// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package server

import (
	"fmt"
	"html/template"
	"log/slog"
	"net"
	"net/http"
	"strconv"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/sessions"
	"github.com/piscsi/piscsi/go/piscsi"
	"github.com/piscsi/piscsi/go/piscsi-web/internal/config"
	"github.com/piscsi/piscsi/go/piscsi-web/internal/driveprops"
	"github.com/piscsi/piscsi/go/piscsi-web/web"
	pb "github.com/piscsi/piscsi/go/proto"
)

// Server represents the HTTP server
type Server struct {
	config       *config.Config
	router       *gin.Engine
	piscsiClient interface {
		SendCommand(cmd *pb.PbCommand) (*pb.PbResult, error)
	}
	sessionStore  *sessions.CookieStore
	driveProps    *driveprops.Properties
	logger        *slog.Logger
	archiveMu     sync.Mutex
	archiveCache  map[string]archiveCacheEntry
	localizer     *web.Catalog
	systemCommand func(name string, args ...string) ([]byte, error)
	hostname      func() string
	hostIP        func() string
}

const (
	readHeaderTimeout = 10 * time.Second
	idleTimeout       = 60 * time.Second
	maxHeaderBytes    = 1 << 20
)

// New creates a new server instance
func New(cfg *config.Config, logger *slog.Logger) (*Server, error) {
	// Set Gin mode based on environment
	gin.SetMode(gin.ReleaseMode)

	router := gin.New()

	// Load HTML templates and the copied gettext catalogs.
	tmpl, err := web.GetTemplates()
	if err != nil || len(tmpl.Templates()) == 0 {
		// Fallback to filesystem for development
		tmpl, err = template.ParseGlob(cfg.TemplatesDir + "/*.html")
		if err != nil {
			return nil, fmt.Errorf("load HTML templates: %w", err)
		}
	}
	localizer, err := web.LoadCatalog()
	if err != nil {
		return nil, err
	}
	router.HTMLRender = web.LocalizedRenderer{Template: tmpl, Catalog: localizer}

	// Add middleware
	router.Use(gin.Recovery())
	router.Use(requestLogger(logger))

	// Create PiSCSI client
	piscsiClient := piscsi.NewClient(cfg.PiscsiHost, cfg.PiscsiPort)

	// Create session store
	sessionStore := sessions.NewCookieStore(
		cfg.SessionCookieHashKey,
		cfg.SessionCookieBlockKey,
	)
	sessionStore.Options = &sessions.Options{
		Path:     "/",
		MaxAge:   cfg.SessionMaxAge,
		HttpOnly: true,
		SameSite: http.SameSiteLaxMode,
	}

	// Load drive properties
	drivePropsPath := cfg.DrivePropertiesFile
	driveProps, err := driveprops.LoadProperties(drivePropsPath)
	if err != nil {
		logger.Warn("Failed to load drive properties", "error", err, "path", drivePropsPath)
		// Continue without drive properties - endpoints will return errors
	} else {
		logger.Info("Loaded drive properties", "path", drivePropsPath)
	}

	server := &Server{
		config:       cfg,
		router:       router,
		piscsiClient: piscsiClient,
		sessionStore: sessionStore,
		driveProps:   driveProps,
		logger:       logger,
		archiveCache: make(map[string]archiveCacheEntry),
		localizer:    localizer,
	}

	if err := server.checkBackendAuthentication(); err != nil {
		return nil, err
	}
	if loaded, err := server.loadDefaultConfiguration(); err != nil {
		logger.Error("Failed to load default configuration", "error", err, "filename", defaultConfigFilename)
	} else if loaded {
		logger.Info("Loaded default configuration", "filename", defaultConfigFilename)
	}

	// Setup routes
	server.setupRoutes()

	return server, nil
}

func (s *Server) checkBackendAuthentication() error {
	result, err := s.piscsiClient.SendCommand(s.newCommandBuilder("en").CheckAuthentication())
	if err != nil {
		return fmt.Errorf("check PiSCSI daemon authentication: %w", err)
	}
	if result.GetStatus() {
		return nil
	}

	if s.config.PiscsiToken == "" {
		return fmt.Errorf("PiSCSI daemon authentication is required; configure --password or PISCSI_TOKEN")
	}
	return fmt.Errorf("PiSCSI daemon authentication failed; --password or PISCSI_TOKEN is invalid: %s", result.GetMsg())
}

func (s *Server) newCommandBuilder(locale string) *piscsi.CommandBuilder {
	builder := piscsi.NewCommandBuilder()
	if s.config != nil {
		builder.SetToken(s.config.PiscsiToken)
	}
	builder.SetLocale(locale)
	return builder
}

// setupRoutes configures all HTTP routes
func (s *Server) setupRoutes() {
	// Serve static files (try embedded first, then filesystem)
	staticFS, err := web.GetStaticFS()
	if err == nil {
		s.router.StaticFS("/static", staticFS)
	} else {
		// Fallback to filesystem for development
		s.router.Static("/static", s.config.StaticDir)
	}

	// Routes that do not need the backend.
	s.router.GET("/healthcheck", s.handleHealthcheck)
	s.router.GET("/pwa/*pwa_path", s.handlePWA)

	// The web interface is intentionally available without a login.
	app := s.router.Group("/")
	{
		// Page routes
		app.GET("/", s.handleIndex)

		// Device operations
		app.POST("/scsi/attach", s.handleAttach)
		app.POST("/scsi/detach", s.handleDetach)
		app.POST("/scsi/detach_all", s.handleDetachAll)
		app.POST("/scsi/eject", s.handleEject)
		app.POST("/scsi/info", s.handleScsiInfo)
		app.POST("/scsi/reserve", s.handleScsiReserve)
		app.POST("/scsi/release", s.handleScsiRelease)

		// File operations
		app.POST("/files/upload", s.handleFilesUpload)
		app.POST("/files/uploadform/", s.handleFilesUploadForm)
		app.GET("/files/download_image", s.handleFilesDownload)
		app.POST("/files/download_image", s.handleFilesDownload)
		app.POST("/files/delete", s.handleFilesDelete)
		app.POST("/files/rename", s.handleFilesRename)
		app.POST("/files/copy", s.handleFilesCopy)
		app.POST("/files/download_config", s.handleFilesDownloadConfig)
		app.POST("/files/create", s.handleFilesCreate)
		app.POST("/files/download_url", s.handleFilesDownloadURL)
		app.POST("/files/create_iso", s.handleFilesCreateISO)
		app.POST("/files/extract_image", s.handleFilesExtractImage)

		// Configuration operations
		app.POST("/config/save", s.handleConfigSave)
		app.POST("/config/load", s.handleConfigLoad)
		app.POST("/config/action", s.handleConfigAction)

		// System operations
		app.POST("/logs/level", s.handleLogsLevel)
		app.POST("/logs/show", s.handleLogsShow)
		app.POST("/sys/reboot", s.handleSysReboot)
		app.POST("/sys/shutdown", s.handleSysShutdown)

		// Drive operations
		app.POST("/drive/create", s.handleDriveCreate)
		app.POST("/drive/cdrom", s.handleDriveCdrom)

		// Page routes
		app.GET("/drive/list", s.handleDriveList)
		app.GET("/sys/admin", s.handleSysAdmin)
		app.GET("/upload", s.handleUploadPage)
		app.POST("/files/diskinfo", s.handleFilesDiskinfo)
		app.GET("/sys/manpage", s.handleSysManpage)

		// Settings endpoints
		app.POST("/language", s.handleLanguage)
		app.GET("/theme", s.handleTheme)
		app.POST("/theme", s.handleTheme)
	}
}

// Start starts the HTTP server
func (s *Server) Start() error {
	address := net.JoinHostPort(s.config.ServerHost, strconv.Itoa(s.config.ServerPort))
	s.logger.Info("Starting PiSCSI web server", "address", address)
	return s.httpServer(address).ListenAndServe()
}

func (s *Server) httpServer(address string) *http.Server {
	return &http.Server{
		Addr:              address,
		Handler:           s.router,
		ReadHeaderTimeout: readHeaderTimeout,
		IdleTimeout:       idleTimeout,
		MaxHeaderBytes:    maxHeaderBytes,
	}
}

// requestLogger logs HTTP requests
func requestLogger(logger *slog.Logger) gin.HandlerFunc {
	return func(c *gin.Context) {
		start := time.Now()
		path := c.Request.URL.Path
		method := c.Request.Method

		c.Next()

		latency := time.Since(start)
		statusCode := c.Writer.Status()

		logger.Info("HTTP request",
			"method", method,
			"path", path,
			"status", statusCode,
			"latency", latency,
			"ip", c.ClientIP(),
		)
	}
}
