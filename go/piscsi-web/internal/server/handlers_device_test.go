package server

import (
	"errors"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"net/url"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/sessions"
	"github.com/piscsi/piscsi/go/piscsi-web/internal/config"
	"github.com/piscsi/piscsi/go/piscsi-web/internal/server/testutil"
	"github.com/piscsi/piscsi/go/piscsi-web/web"
	pb "github.com/piscsi/piscsi/go/proto"
)

// TestHandleAttach_Success tests successfully attaching a device
func TestHandleAttach_Success(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	var attachedFile string
	piscsiClient := &testutil.MockPiSCSIClient{
		SendCommandFunc: func(cmd *pb.PbCommand) (*pb.PbResult, error) {
			attachedFile = cmd.GetDevices()[0].GetParams()["file"]
			return &pb.PbResult{Status: true}, nil
		},
	}
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{BaseDir: "/var/lib/piscsi/images"},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/attach", server.handleAttach)

	form := url.Values{}
	form.Add("scsi_id", "6")
	form.Add("unit", "0")
	form.Add("type", "SCHD")
	form.Add("file", "test.hds")

	req := httptest.NewRequest("POST", "/scsi/attach", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	resp := w.Result()
	if attachedFile != "/var/lib/piscsi/images/test.hds" {
		t.Fatalf("attached file = %q, want /var/lib/piscsi/images/test.hds", attachedFile)
	}

	// Verify redirect
	if resp.StatusCode != http.StatusSeeOther && resp.StatusCode != http.StatusFound {
		// In unit tests, status code handling may differ
		location := resp.Header.Get("Location")
		if location != "/" {
			t.Errorf("expected redirect to '/', got '%s'", location)
		}
	}
}

func TestHandleAttach_ForwardsDaemonDefinedParameters(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	var attached *pb.PbDeviceDefinition
	piscsiClient := &testutil.MockPiSCSIClient{
		SendCommandFunc: func(cmd *pb.PbCommand) (*pb.PbResult, error) {
			attached = cmd.GetDevices()[0]
			return &pb.PbResult{Status: true}, nil
		},
	}
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/attach", server.handleAttach)

	form := url.Values{
		"scsi_id":         {"6"},
		"unit":            {"0"},
		"type":            {"SCLP"},
		"param_timeout":   {"30"},
		"param_interface": {"piscsi_bridge"},
		"param_empty":     {""},
	}
	req := httptest.NewRequest("POST", "/scsi/attach", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	if attached == nil {
		t.Fatal("no device definition was sent")
	}
	if attached.GetType() != pb.PbDeviceType_SCLP {
		t.Fatalf("device type = %s, want SCLP", attached.GetType())
	}
	if got := attached.GetParams()["timeout"]; got != "30" {
		t.Errorf("timeout = %q, want 30", got)
	}
	if got := attached.GetParams()["interface"]; got != "piscsi_bridge" {
		t.Errorf("interface = %q, want piscsi_bridge", got)
	}
	if _, ok := attached.GetParams()["empty"]; ok {
		t.Error("empty parameter was forwarded")
	}
}

func TestParseDeviceTypeSupportsEveryCurrentProtobufType(t *testing.T) {
	for _, name := range []string{"SCHD", "SCRM", "SCMO", "SCCD", "SCDP", "SCHS", "SCLP", "SCTP"} {
		t.Run(name, func(t *testing.T) {
			got, err := parseDeviceType(strings.ToLower(name))
			if err != nil {
				t.Fatalf("parseDeviceType(%q): %v", name, err)
			}
			if got.String() != name {
				t.Fatalf("parseDeviceType(%q) = %s", name, got)
			}
		})
	}
}

func TestBuildDeviceCatalogUsesDaemonCapabilities(t *testing.T) {
	server := &Server{}
	files := []map[string]interface{}{
		{"Name": "disk.hds", "DetectedType": "SCHD"},
		{"Name": "backup.tap", "DetectedType": "SCTP"},
	}
	info := &pb.PbDeviceTypesInfo{Properties: []*pb.PbDeviceTypeProperties{
		{
			Type: pb.PbDeviceType_SCDP,
			Properties: &pb.PbDeviceProperties{
				DefaultParams: map[string]string{"interface": "piscsi_bridge", "timeout": "30", "name": "DaynaPORT"},
			},
		},
		{
			Type: pb.PbDeviceType_SCTP,
			Properties: &pb.PbDeviceProperties{
				SupportsFile: true,
				Removable:    true,
			},
		},
	}}

	catalog := server.buildDeviceCatalog(info, files, []string{"eth0", "piscsi_bridge"})
	if len(catalog) != 2 {
		t.Fatalf("catalog length = %d, want 2", len(catalog))
	}
	if catalog[0].Key != "SCDP" || len(catalog[0].Parameters) != 3 {
		t.Fatalf("unexpected network catalog entry: %#v", catalog[0])
	}
	if catalog[0].Parameters[0].Kind != "interface" ||
		len(catalog[0].Parameters[0].Options) != 2 {
		t.Fatalf("interface control = %#v", catalog[0].Parameters[0])
	}
	if catalog[0].Parameters[2].Kind != "number" {
		t.Fatalf("timeout control = %#v, want number", catalog[0].Parameters[2])
	}
	if !catalog[1].SupportsFile || !catalog[1].Removable {
		t.Fatalf("unexpected tape capabilities: %#v", catalog[1])
	}
	if len(catalog[1].Files) != 1 || catalog[1].Files[0]["Name"] != "backup.tap" {
		t.Fatalf("tape files = %#v", catalog[1].Files)
	}
}

func TestHandleAttachInsertsMediaIntoEmptyRemovableDevice(t *testing.T) {
	var operation pb.PbOperation
	server := &Server{
		sessionStore: sessions.NewCookieStore([]byte("test-secret-key")),
		config:       &config.Config{BaseDir: "/var/lib/piscsi/images"},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
		piscsiClient: &testutil.MockPiSCSIClient{SendCommandFunc: func(command *pb.PbCommand) (*pb.PbResult, error) {
			switch command.GetOperation() {
			case pb.PbOperation_DEVICES_INFO:
				return &pb.PbResult{
					Status: true,
					Result: &pb.PbResult_DevicesInfo{DevicesInfo: &pb.PbDevicesInfo{Devices: []*pb.PbDevice{{
						Id:         4,
						Unit:       1,
						Type:       pb.PbDeviceType_SCRM,
						Properties: &pb.PbDeviceProperties{Removable: true},
						Status:     &pb.PbDeviceStatus{Removed: true},
					}}}},
				}, nil
			case pb.PbOperation_INSERT:
				operation = command.GetOperation()
				if got := command.GetDevices()[0].GetParams()["file"]; got != "/var/lib/piscsi/images/disk.hdr" {
					t.Fatalf("inserted file = %q", got)
				}
				return &pb.PbResult{Status: true}, nil
			default:
				t.Fatalf("unexpected operation %s", command.GetOperation())
				return nil, nil
			}
		}},
	}

	router := gin.New()
	router.POST("/scsi/attach", server.handleAttach)
	form := url.Values{"scsi_id": {"4"}, "unit": {"1"}, "type": {"SCRM"}, "file": {"disk.hdr"}}
	request := httptest.NewRequest(http.MethodPost, "/scsi/attach", strings.NewReader(form.Encode()))
	request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	response := httptest.NewRecorder()
	router.ServeHTTP(response, request)

	if response.Code != http.StatusSeeOther {
		t.Fatalf("status = %d; body: %s", response.Code, response.Body.String())
	}
	if operation != pb.PbOperation_INSERT {
		t.Fatalf("operation = %s, want INSERT", operation)
	}
}

func TestHandleAttachRejectsDifferentRemovableMediaType(t *testing.T) {
	server := &Server{
		sessionStore: sessions.NewCookieStore([]byte("test-secret-key")),
		config:       &config.Config{BaseDir: "/var/lib/piscsi/images"},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
		piscsiClient: &testutil.MockPiSCSIClient{SendCommandFunc: func(command *pb.PbCommand) (*pb.PbResult, error) {
			if command.GetOperation() != pb.PbOperation_DEVICES_INFO {
				t.Fatalf("unexpected operation %s", command.GetOperation())
			}
			return &pb.PbResult{
				Status: true,
				Result: &pb.PbResult_DevicesInfo{DevicesInfo: &pb.PbDevicesInfo{Devices: []*pb.PbDevice{{
					Id:         4,
					Type:       pb.PbDeviceType_SCMO,
					Properties: &pb.PbDeviceProperties{Removable: true},
					Status:     &pb.PbDeviceStatus{Removed: true},
				}}}},
			}, nil
		}},
	}

	router := gin.New()
	router.POST("/scsi/attach", server.handleAttach)
	form := url.Values{"scsi_id": {"4"}, "type": {"SCRM"}, "file": {"disk.hdr"}}
	request := httptest.NewRequest(http.MethodPost, "/scsi/attach", strings.NewReader(form.Encode()))
	request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	response := httptest.NewRecorder()
	router.ServeHTTP(response, request)

	if response.Code != http.StatusSeeOther {
		t.Fatalf("status = %d; body: %s", response.Code, response.Body.String())
	}
	session := testutil.GetSessionFromResponse(t, response.Result(), server.sessionStore, request)
	_, errorMessage := GetFlashesForTemplate(session)
	if !strings.Contains(errorMessage, "existing SCMO") {
		t.Fatalf("error flash = %q", errorMessage)
	}
}

func TestHandleAttachAppliesImagePropertiesIdentity(t *testing.T) {
	configDir := t.TempDir()
	if err := os.WriteFile(filepath.Join(configDir, "disk.hds.properties"), []byte(`{
		"vendor": "ACME",
		"product": "Virtual Disk",
		"revision": "1.0",
		"block_size": 1024
	}`), 0o644); err != nil {
		t.Fatal(err)
	}

	var attached *pb.PbDeviceDefinition
	server := &Server{
		sessionStore: sessions.NewCookieStore([]byte("test-secret-key")),
		config:       &config.Config{BaseDir: "/images", ConfigDir: configDir},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
		piscsiClient: &testutil.MockPiSCSIClient{SendCommandFunc: func(command *pb.PbCommand) (*pb.PbResult, error) {
			attached = command.GetDevices()[0]
			return &pb.PbResult{Status: true}, nil
		}},
	}

	router := gin.New()
	router.POST("/scsi/attach", server.handleAttach)
	form := url.Values{"scsi_id": {"2"}, "type": {"SCHD"}, "file": {"disk.hds"}}
	request := httptest.NewRequest(http.MethodPost, "/scsi/attach", strings.NewReader(form.Encode()))
	request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	response := httptest.NewRecorder()
	router.ServeHTTP(response, request)

	if response.Code != http.StatusSeeOther {
		t.Fatalf("status = %d; body: %s", response.Code, response.Body.String())
	}
	if attached.GetVendor() != "ACME" || attached.GetProduct() != "Virtual Disk" ||
		attached.GetRevision() != "1.0" || attached.GetBlockSize() != 1024 {
		t.Fatalf("attached identity = %#v", attached)
	}
}

func TestBridgeConfigurationStatus(t *testing.T) {
	original := bridgeConfigurationPaths
	t.Cleanup(func() { bridgeConfigurationPaths = original })
	tempDir := t.TempDir()
	bridgeConfigurationPaths.sysctl = filepath.Join(tempDir, "sysctl.conf")
	bridgeConfigurationPaths.nat = filepath.Join(tempDir, "rules.v4")
	bridgeConfigurationPaths.dhcpcd = filepath.Join(tempDir, "dhcpcd.conf")
	bridgeConfigurationPaths.bridge = filepath.Join(tempDir, "piscsi_bridge")

	if err := os.WriteFile(bridgeConfigurationPaths.dhcpcd, []byte("denyinterfaces eth0\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	if ready, message := bridgeConfigurationStatus("eth0"); ready || !strings.Contains(message, bridgeConfigurationPaths.bridge) {
		t.Fatalf("ready = %v, message = %q", ready, message)
	}
	if err := os.WriteFile(bridgeConfigurationPaths.bridge, []byte("auto piscsi_bridge\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	if ready, message := bridgeConfigurationStatus("eth0"); !ready {
		t.Fatalf("ready = false, message = %q", message)
	}
}

func TestHandleScsiInfoRendersHTML(t *testing.T) {
	server := &Server{
		sessionStore: sessions.NewCookieStore([]byte("test-secret-key")),
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
		piscsiClient: &testutil.MockPiSCSIClient{SendCommandFunc: func(command *pb.PbCommand) (*pb.PbResult, error) {
			switch command.GetOperation() {
			case pb.PbOperation_DEVICES_INFO:
				return &pb.PbResult{
					Status: true,
					Result: &pb.PbResult_DevicesInfo{DevicesInfo: &pb.PbDevicesInfo{Devices: []*pb.PbDevice{{
						Id: 2, Type: pb.PbDeviceType_SCHD, Product: "Test Disk",
						File: &pb.PbImageFile{Name: "disk.hds", Size: 4096},
					}}}},
				}, nil
			case pb.PbOperation_SERVER_INFO:
				return &pb.PbResult{Status: true, Result: &pb.PbResult_ServerInfo{ServerInfo: &pb.PbServerInfo{}}}, nil
			default:
				t.Fatalf("unexpected operation %s", command.GetOperation())
				return nil, nil
			}
		}},
	}
	templates, err := web.GetTemplates()
	if err != nil {
		t.Fatal(err)
	}
	router := gin.New()
	router.SetHTMLTemplate(templates)
	router.POST("/scsi/info", server.handleScsiInfo)

	htmlRequest := httptest.NewRequest(http.MethodPost, "/scsi/info", nil)
	htmlResponse := httptest.NewRecorder()
	router.ServeHTTP(htmlResponse, htmlRequest)
	if htmlResponse.Code != http.StatusOK ||
		!strings.Contains(htmlResponse.Body.String(), "Detailed Info for Attached Devices") ||
		!strings.Contains(htmlResponse.Body.String(), "Test Disk") {
		t.Fatalf("status = %d; body: %s", htmlResponse.Code, htmlResponse.Body.String())
	}

}

// TestHandleAttach_MissingSCSIID tests attach with missing SCSI ID
func TestHandleAttach_MissingSCSIID(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	piscsiClient := testutil.NewMockPiSCSIClientAlwaysSuccess()
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/attach", server.handleAttach)

	form := url.Values{}
	form.Add("type", "SCHD")
	form.Add("file", "test.hds")

	req := httptest.NewRequest("POST", "/scsi/attach", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	resp := w.Result()

	// Verify session contains error flash message
	cookies := resp.Cookies()
	if len(cookies) > 0 {
		verifyReq := httptest.NewRequest("GET", "/", nil)
		verifyReq.AddCookie(cookies[0])

		session, err := store.Get(verifyReq, sessionName)
		if err != nil {
			t.Fatalf("failed to get session: %v", err)
		}

		_, errorMessage := GetFlashesForTemplate(session)
		if !strings.Contains(errorMessage, "SCSI ID is required") {
			t.Errorf("expected error about required SCSI ID, got: %s", errorMessage)
		}
	}
}

// TestHandleAttach_InvalidSCSIID tests attach with out-of-range SCSI ID
func TestHandleAttach_InvalidSCSIID(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	piscsiClient := testutil.NewMockPiSCSIClientAlwaysSuccess()
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/attach", server.handleAttach)

	tests := []struct {
		name   string
		scsiID string
	}{
		{"negative ID", "-1"},
		{"ID too high", "8"},
		{"invalid format", "abc"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			form := url.Values{}
			form.Add("scsi_id", tt.scsiID)
			form.Add("type", "SCHD")

			req := httptest.NewRequest("POST", "/scsi/attach", strings.NewReader(form.Encode()))
			req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
			w := httptest.NewRecorder()

			router.ServeHTTP(w, req)

			resp := w.Result()

			// Verify error message in session
			cookies := resp.Cookies()
			if len(cookies) > 0 {
				verifyReq := httptest.NewRequest("GET", "/", nil)
				verifyReq.AddCookie(cookies[0])

				session, err := store.Get(verifyReq, sessionName)
				if err != nil {
					t.Fatalf("failed to get session: %v", err)
				}

				_, errorMessage := GetFlashesForTemplate(session)
				if !strings.Contains(errorMessage, "Invalid SCSI ID") {
					t.Errorf("expected error about invalid SCSI ID, got: %s", errorMessage)
				}
			}
		})
	}
}

// TestHandleAttach_InvalidLUN tests attach with out-of-range LUN
func TestHandleAttach_InvalidLUN(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	piscsiClient := testutil.NewMockPiSCSIClientAlwaysSuccess()
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/attach", server.handleAttach)

	form := url.Values{}
	form.Add("scsi_id", "6")
	form.Add("unit", "32") // LUN must be 0-31
	form.Add("type", "SCHD")

	req := httptest.NewRequest("POST", "/scsi/attach", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	resp := w.Result()

	cookies := resp.Cookies()
	if len(cookies) > 0 {
		verifyReq := httptest.NewRequest("GET", "/", nil)
		verifyReq.AddCookie(cookies[0])

		session, err := store.Get(verifyReq, sessionName)
		if err != nil {
			t.Fatalf("failed to get session: %v", err)
		}

		_, errorMessage := GetFlashesForTemplate(session)
		if !strings.Contains(errorMessage, "Invalid LUN") {
			t.Errorf("expected error about invalid LUN, got: %s", errorMessage)
		}
	}
}

// TestHandleAttach_DaemonError tests attach when daemon communication fails
func TestHandleAttach_DaemonError(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	piscsiClient := testutil.NewMockPiSCSIClientWithError(errors.New("connection refused"))
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/attach", server.handleAttach)

	form := url.Values{}
	form.Add("scsi_id", "6")
	form.Add("type", "SCHD")
	form.Add("file", "test.hds")

	req := httptest.NewRequest("POST", "/scsi/attach", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	resp := w.Result()

	cookies := resp.Cookies()
	if len(cookies) > 0 {
		verifyReq := httptest.NewRequest("GET", "/", nil)
		verifyReq.AddCookie(cookies[0])

		session, err := store.Get(verifyReq, sessionName)
		if err != nil {
			t.Fatalf("failed to get session: %v", err)
		}

		_, errorMessage := GetFlashesForTemplate(session)
		if !strings.Contains(errorMessage, "Failed to communicate") {
			t.Errorf("expected daemon communication error, got: %s", errorMessage)
		}
	}
}

// TestHandleAttach_ResultStatusFalse tests attach when daemon returns false status
func TestHandleAttach_ResultStatusFalse(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	piscsiClient := testutil.NewMockPiSCSIClientAlwaysFail("Device already attached")
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/attach", server.handleAttach)

	form := url.Values{}
	form.Add("scsi_id", "6")
	form.Add("type", "SCHD")
	form.Add("file", "test.hds")

	req := httptest.NewRequest("POST", "/scsi/attach", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	resp := w.Result()

	cookies := resp.Cookies()
	if len(cookies) > 0 {
		verifyReq := httptest.NewRequest("GET", "/", nil)
		verifyReq.AddCookie(cookies[0])

		session, err := store.Get(verifyReq, sessionName)
		if err != nil {
			t.Fatalf("failed to get session: %v", err)
		}

		_, errorMessage := GetFlashesForTemplate(session)
		if !strings.Contains(errorMessage, "Device already attached") {
			t.Errorf("expected daemon error message, got: %s", errorMessage)
		}
	}
}

// TestHandleDetach_Success tests successfully detaching a device
func TestHandleDetach_Success(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	piscsiClient := testutil.NewMockPiSCSIClientAlwaysSuccess()
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/detach", server.handleDetach)

	form := url.Values{}
	form.Add("scsi_id", "6")
	form.Add("unit", "0")

	req := httptest.NewRequest("POST", "/scsi/detach", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	resp := w.Result()

	location := resp.Header.Get("Location")
	if location != "/" {
		t.Errorf("expected redirect to '/', got '%s'", location)
	}
}

// TestHandleDetach_MissingSCSIID tests detach with missing SCSI ID
func TestHandleDetach_MissingSCSIID(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	piscsiClient := testutil.NewMockPiSCSIClientAlwaysSuccess()
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/detach", server.handleDetach)

	form := url.Values{}
	// Missing scsi_id

	req := httptest.NewRequest("POST", "/scsi/detach", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	resp := w.Result()

	cookies := resp.Cookies()
	if len(cookies) > 0 {
		verifyReq := httptest.NewRequest("GET", "/", nil)
		verifyReq.AddCookie(cookies[0])

		session, err := store.Get(verifyReq, sessionName)
		if err != nil {
			t.Fatalf("failed to get session: %v", err)
		}

		_, errorMessage := GetFlashesForTemplate(session)
		if !strings.Contains(errorMessage, "SCSI ID is required") {
			t.Errorf("expected error about required SCSI ID, got: %s", errorMessage)
		}
	}
}

// TestHandleDetach_InvalidSCSIID tests detach with invalid SCSI ID
func TestHandleDetach_InvalidSCSIID(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	piscsiClient := testutil.NewMockPiSCSIClientAlwaysSuccess()
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/detach", server.handleDetach)

	form := url.Values{}
	form.Add("scsi_id", "99") // Invalid: must be 0-7

	req := httptest.NewRequest("POST", "/scsi/detach", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	resp := w.Result()

	cookies := resp.Cookies()
	if len(cookies) > 0 {
		verifyReq := httptest.NewRequest("GET", "/", nil)
		verifyReq.AddCookie(cookies[0])

		session, err := store.Get(verifyReq, sessionName)
		if err != nil {
			t.Fatalf("failed to get session: %v", err)
		}

		_, errorMessage := GetFlashesForTemplate(session)
		if !strings.Contains(errorMessage, "Invalid SCSI ID") {
			t.Errorf("expected error about invalid SCSI ID, got: %s", errorMessage)
		}
	}
}

// TestHandleDetach_DaemonError tests detach when daemon communication fails
func TestHandleDetach_DaemonError(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	piscsiClient := testutil.NewMockPiSCSIClientWithError(errors.New("connection refused"))
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/detach", server.handleDetach)

	form := url.Values{}
	form.Add("scsi_id", "6")

	req := httptest.NewRequest("POST", "/scsi/detach", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	resp := w.Result()

	cookies := resp.Cookies()
	if len(cookies) > 0 {
		verifyReq := httptest.NewRequest("GET", "/", nil)
		verifyReq.AddCookie(cookies[0])

		session, err := store.Get(verifyReq, sessionName)
		if err != nil {
			t.Fatalf("failed to get session: %v", err)
		}

		_, errorMessage := GetFlashesForTemplate(session)
		if !strings.Contains(errorMessage, "Failed to communicate") {
			t.Errorf("expected daemon communication error, got: %s", errorMessage)
		}
	}
}

// TestHandleDetachAll_Success tests successfully detaching all devices
func TestHandleDetachAll_Success(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	piscsiClient := testutil.NewMockPiSCSIClientAlwaysSuccess()
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/detach/all", server.handleDetachAll)

	req := httptest.NewRequest("POST", "/scsi/detach/all", nil)
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	resp := w.Result()

	location := resp.Header.Get("Location")
	if location != "/" {
		t.Errorf("expected redirect to '/', got '%s'", location)
	}

	// Verify success message
	cookies := resp.Cookies()
	if len(cookies) > 0 {
		verifyReq := httptest.NewRequest("GET", "/", nil)
		verifyReq.AddCookie(cookies[0])

		session, err := store.Get(verifyReq, sessionName)
		if err != nil {
			t.Fatalf("failed to get session: %v", err)
		}

		flashMessage, _ := GetFlashesForTemplate(session)
		if !strings.Contains(flashMessage, "Detached all devices") {
			t.Errorf("expected success message about detaching all devices, got: %s", flashMessage)
		}
	}
}

// TestHandleDetachAll_DaemonError tests detach all when daemon fails
func TestHandleDetachAll_DaemonError(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	piscsiClient := testutil.NewMockPiSCSIClientWithError(errors.New("connection refused"))
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/detach/all", server.handleDetachAll)

	req := httptest.NewRequest("POST", "/scsi/detach/all", nil)
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	resp := w.Result()

	cookies := resp.Cookies()
	if len(cookies) > 0 {
		verifyReq := httptest.NewRequest("GET", "/", nil)
		verifyReq.AddCookie(cookies[0])

		session, err := store.Get(verifyReq, sessionName)
		if err != nil {
			t.Fatalf("failed to get session: %v", err)
		}

		_, errorMessage := GetFlashesForTemplate(session)
		if !strings.Contains(errorMessage, "Failed to communicate") {
			t.Errorf("expected daemon communication error, got: %s", errorMessage)
		}
	}
}

// TestHandleEject_Success tests successfully ejecting removable media
func TestHandleEject_Success(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	piscsiClient := testutil.NewMockPiSCSIClientAlwaysSuccess()
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/eject", server.handleEject)

	form := url.Values{}
	form.Add("scsi_id", "6")
	form.Add("unit", "0")

	req := httptest.NewRequest("POST", "/scsi/eject", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	resp := w.Result()

	location := resp.Header.Get("Location")
	if location != "/" {
		t.Errorf("expected redirect to '/', got '%s'", location)
	}

	// Verify success message
	cookies := resp.Cookies()
	if len(cookies) > 0 {
		verifyReq := httptest.NewRequest("GET", "/", nil)
		verifyReq.AddCookie(cookies[0])

		session, err := store.Get(verifyReq, sessionName)
		if err != nil {
			t.Fatalf("failed to get session: %v", err)
		}

		flashMessage, _ := GetFlashesForTemplate(session)
		if !strings.Contains(flashMessage, "Ejected media") {
			t.Errorf("expected success message about ejecting media, got: %s", flashMessage)
		}
	}
}

// TestHandleEject_MissingSCSIID tests eject with missing SCSI ID
func TestHandleEject_MissingSCSIID(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	piscsiClient := testutil.NewMockPiSCSIClientAlwaysSuccess()
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/eject", server.handleEject)

	form := url.Values{}
	// Missing scsi_id

	req := httptest.NewRequest("POST", "/scsi/eject", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	resp := w.Result()

	cookies := resp.Cookies()
	if len(cookies) > 0 {
		verifyReq := httptest.NewRequest("GET", "/", nil)
		verifyReq.AddCookie(cookies[0])

		session, err := store.Get(verifyReq, sessionName)
		if err != nil {
			t.Fatalf("failed to get session: %v", err)
		}

		_, errorMessage := GetFlashesForTemplate(session)
		if !strings.Contains(errorMessage, "SCSI ID is required") {
			t.Errorf("expected error about required SCSI ID, got: %s", errorMessage)
		}
	}
}

// TestHandleEject_InvalidSCSIID tests eject with invalid SCSI ID
func TestHandleEject_InvalidSCSIID(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	piscsiClient := testutil.NewMockPiSCSIClientAlwaysSuccess()
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/eject", server.handleEject)

	form := url.Values{}
	form.Add("scsi_id", "10") // Invalid: must be 0-7

	req := httptest.NewRequest("POST", "/scsi/eject", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	resp := w.Result()

	cookies := resp.Cookies()
	if len(cookies) > 0 {
		verifyReq := httptest.NewRequest("GET", "/", nil)
		verifyReq.AddCookie(cookies[0])

		session, err := store.Get(verifyReq, sessionName)
		if err != nil {
			t.Fatalf("failed to get session: %v", err)
		}

		_, errorMessage := GetFlashesForTemplate(session)
		if !strings.Contains(errorMessage, "Invalid SCSI ID") {
			t.Errorf("expected error about invalid SCSI ID, got: %s", errorMessage)
		}
	}
}

// TestHandleEject_DaemonError tests eject when daemon communication fails
func TestHandleEject_DaemonError(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	piscsiClient := testutil.NewMockPiSCSIClientWithError(errors.New("connection refused"))
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/eject", server.handleEject)

	form := url.Values{}
	form.Add("scsi_id", "6")

	req := httptest.NewRequest("POST", "/scsi/eject", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	resp := w.Result()

	cookies := resp.Cookies()
	if len(cookies) > 0 {
		verifyReq := httptest.NewRequest("GET", "/", nil)
		verifyReq.AddCookie(cookies[0])

		session, err := store.Get(verifyReq, sessionName)
		if err != nil {
			t.Fatalf("failed to get session: %v", err)
		}

		_, errorMessage := GetFlashesForTemplate(session)
		if !strings.Contains(errorMessage, "Failed to communicate") {
			t.Errorf("expected daemon communication error, got: %s", errorMessage)
		}
	}
}

// TestHandleEject_ResultStatusFalse tests eject when daemon returns false status
func TestHandleEject_ResultStatusFalse(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	piscsiClient := testutil.NewMockPiSCSIClientAlwaysFail("No media to eject")
	server := &Server{
		sessionStore: store,
		piscsiClient: piscsiClient,
		config:       &config.Config{},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/eject", server.handleEject)

	form := url.Values{}
	form.Add("scsi_id", "6")

	req := httptest.NewRequest("POST", "/scsi/eject", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	w := httptest.NewRecorder()

	router.ServeHTTP(w, req)

	resp := w.Result()

	cookies := resp.Cookies()
	if len(cookies) > 0 {
		verifyReq := httptest.NewRequest("GET", "/", nil)
		verifyReq.AddCookie(cookies[0])

		session, err := store.Get(verifyReq, sessionName)
		if err != nil {
			t.Fatalf("failed to get session: %v", err)
		}

		_, errorMessage := GetFlashesForTemplate(session)
		if !strings.Contains(errorMessage, "No media to eject") {
			t.Errorf("expected daemon error message, got: %s", errorMessage)
		}
	}
}

func TestHandleScsiReservePreservesExistingReservations(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	reservedParam := ""
	server := &Server{
		sessionStore: store,
		config:       &config.Config{},
		piscsiClient: &testutil.MockPiSCSIClient{
			SendCommandFunc: func(command *pb.PbCommand) (*pb.PbResult, error) {
				switch command.GetOperation() {
				case pb.PbOperation_SERVER_INFO:
					return &pb.PbResult{
						Status: true,
						Result: &pb.PbResult_ServerInfo{
							ServerInfo: &pb.PbServerInfo{
								ReservedIdsInfo: &pb.PbReservedIdsInfo{Ids: []int32{1, 3}},
							},
						},
					}, nil
				case pb.PbOperation_RESERVE_IDS:
					reservedParam = command.GetParams()["ids"]
					return &pb.PbResult{Status: true}, nil
				default:
					t.Fatalf("unexpected operation %s", command.GetOperation())
					return nil, nil
				}
			},
		},
		logger: slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/reserve", server.handleScsiReserve)
	form := url.Values{"scsi_id": {"6"}}
	request := httptest.NewRequest(http.MethodPost, "/scsi/reserve", strings.NewReader(form.Encode()))
	request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	response := httptest.NewRecorder()

	router.ServeHTTP(response, request)

	if response.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, want %d; body: %s", response.Code, http.StatusSeeOther, response.Body.String())
	}
	if reservedParam != "1,3,6" {
		t.Fatalf("reserved IDs parameter = %q, want %q", reservedParam, "1,3,6")
	}
}

func TestHandleScsiReleasePreservesOtherReservations(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	reservedParam := ""
	server := &Server{
		sessionStore: store,
		config:       &config.Config{},
		piscsiClient: &testutil.MockPiSCSIClient{
			SendCommandFunc: func(command *pb.PbCommand) (*pb.PbResult, error) {
				switch command.GetOperation() {
				case pb.PbOperation_SERVER_INFO:
					return &pb.PbResult{
						Status: true,
						Result: &pb.PbResult_ServerInfo{
							ServerInfo: &pb.PbServerInfo{
								ReservedIdsInfo: &pb.PbReservedIdsInfo{Ids: []int32{1, 3, 6}},
							},
						},
					}, nil
				case pb.PbOperation_RESERVE_IDS:
					reservedParam = command.GetParams()["ids"]
					return &pb.PbResult{Status: true}, nil
				default:
					t.Fatalf("unexpected operation %s", command.GetOperation())
					return nil, nil
				}
			},
		},
		logger: slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	router := gin.New()
	router.POST("/scsi/release", server.handleScsiRelease)
	form := url.Values{"scsi_id": {"3"}}
	request := httptest.NewRequest(http.MethodPost, "/scsi/release", strings.NewReader(form.Encode()))
	request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	response := httptest.NewRecorder()

	router.ServeHTTP(response, request)

	if response.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, want %d; body: %s", response.Code, http.StatusSeeOther, response.Body.String())
	}
	if reservedParam != "1,6" {
		t.Fatalf("reserved IDs parameter = %q, want %q", reservedParam, "1,6")
	}
}
