package server

import (
	"io"
	"log/slog"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/piscsi/piscsi-web/internal/config"
)

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
