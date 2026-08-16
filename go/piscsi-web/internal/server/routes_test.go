package server

import (
	"io"
	"log/slog"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/piscsi/piscsi/go/piscsi-web/internal/config"
)

func TestHTTPServerConfiguration(t *testing.T) {
	router := gin.New()
	server := (&Server{router: router}).httpServer("127.0.0.1:8080")

	if server.Addr != "127.0.0.1:8080" {
		t.Errorf("address = %q, want %q", server.Addr, "127.0.0.1:8080")
	}
	if server.Handler != router {
		t.Error("handler does not use the Gin router")
	}
	if server.ReadHeaderTimeout != 10*time.Second {
		t.Errorf("read header timeout = %s, want %s", server.ReadHeaderTimeout, 10*time.Second)
	}
	if server.IdleTimeout != time.Minute {
		t.Errorf("idle timeout = %s, want %s", server.IdleTimeout, time.Minute)
	}
	if server.MaxHeaderBytes != 1<<20 {
		t.Errorf("maximum header bytes = %d, want %d", server.MaxHeaderBytes, 1<<20)
	}
}

func TestRoutesRequireNoBrowserLogin(t *testing.T) {
	gin.SetMode(gin.TestMode)
	server := &Server{
		config: &config.Config{
			StaticDir: "testdata",
		},
		router: gin.New(),
		logger: slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	server.setupRoutes()

	routes := make(map[string]bool)
	for _, route := range server.router.Routes() {
		routes[route.Method+" "+route.Path] = true
	}

	if !routes["GET /"] {
		t.Fatal("GET / is not registered")
	}
	if routes["GET /login"] || routes["POST /login"] || routes["GET /logout"] {
		t.Fatal("browser authentication routes are still registered")
	}
}
