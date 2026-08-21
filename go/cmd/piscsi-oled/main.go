package main

import (
	"context"
	"flag"
	"fmt"
	"log/slog"
	"os"
	"os/signal"
	"slices"
	"syscall"
	"time"

	"github.com/piscsi/piscsi/go/piscsi"
	oled "github.com/piscsi/piscsi/go/piscsi-oled"
)

func main() {
	version := flag.Bool("v", false, "print version information and exit")
	rotation := flag.Int("rotation", 180, "screen rotation in degrees (0 or 180)")
	height := flag.Int("height", 32, "screen height (32 or 64 pixels)")
	refresh := flag.Int("refresh_interval", 1000, "screen refresh interval in milliseconds")
	horizontalScrollStep := flag.Int("horizontal_scroll_step", oled.DefaultHorizontalScrollStep, "horizontal scroll pixels per screen refresh")
	password := flag.String("password", "", "token password for authenticating with the PiSCSI daemon")
	host := flag.String("host", piscsi.DefaultHost, "PiSCSI daemon hostname")
	port := flag.Int("port", piscsi.DefaultPort, "PiSCSI daemon port")
	i2cDevice := flag.String("i2c-device", "/dev/i2c-1", "Linux I2C device")
	i2cAddress := flag.Int("i2c-address", 0x3c, "SSD1306 I2C address")
	screensaverMode := flag.String("screensaver", "off", "screensaver mode (off, ip, or blank)")
	screensaverIdleTimeout := flag.Duration("screensaver-idle-timeout", 5*time.Minute, "idle time before screensaver activates")
	screensaverMoveInterval := flag.Duration("screensaver-move-interval", 30*time.Second, "interval between screensaver line moves")
	diagnostic := flag.Bool("diagnostic", false, "show start and stop splashes, then exit")
	flag.Parse()
	if *version {
		fmt.Print(piscsi.VersionBanner("PiSCSI OLED Display"))
		return
	}

	logger := slog.New(slog.NewTextHandler(os.Stdout, nil))
	slog.SetDefault(logger)
	if *refresh < 0 {
		logger.Error("refresh_interval must not be negative")
		os.Exit(2)
	}
	if *horizontalScrollStep < 0 {
		logger.Error("horizontal_scroll_step must not be negative")
		os.Exit(2)
	}
	var ipScreensaver *oled.IPScreenSaver
	var blankScreensaver *oled.BlankScreenSaver
	switch *screensaverMode {
	case "off":
	case "ip":
		var err error
		ipScreensaver, err = oled.NewIPScreenSaver(*screensaverIdleTimeout, *screensaverMoveInterval)
		if err != nil {
			logger.Error("invalid screensaver configuration", "error", err)
			os.Exit(2)
		}
	case "blank":
		var err error
		blankScreensaver, err = oled.NewBlankScreenSaver(*screensaverIdleTimeout)
		if err != nil {
			logger.Error("invalid screensaver configuration", "error", err)
			os.Exit(2)
		}
	default:
		logger.Error("unsupported screensaver mode", "mode", *screensaverMode)
		os.Exit(2)
	}
	renderer, err := oled.NewRenderer(*height, *rotation)
	if err != nil {
		logger.Error("invalid display configuration", "error", err)
		os.Exit(2)
	}
	defer renderer.Close()
	display, err := oled.NewSSD1306(oled.SSD1306Config{
		Device: *i2cDevice, Address: *i2cAddress, Height: *height,
		// Rotation is applied by Renderer so golden tests exercise the exact
		// screen content that reaches the transport.
		Rotation: 0,
	})
	if err != nil {
		logger.Error("create display", "error", err)
		os.Exit(2)
	}
	if err := display.Init(); err != nil {
		logger.Error("initialize display", "error", err)
		os.Exit(1)
	}
	defer display.Close()

	start, err := renderer.Splash(true)
	if err != nil {
		logger.Error("render start splash", "error", err)
		os.Exit(1)
	}
	if err := display.Present(start); err != nil {
		logger.Error("show start splash", "error", err)
		os.Exit(1)
	}
	if *diagnostic {
		time.Sleep(700 * time.Millisecond)
		shutdown(display, renderer, logger)
		return
	}
	time.Sleep(2 * time.Second)

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()
	monitor := oled.NewMonitor(piscsi.NewClient(*host, *port), *password)
	if err := monitor.LoadDeviceTypes(ctx); err != nil {
		logger.Warn("could not cache device type metadata", "error", err)
	}

	interval := time.Duration(*refresh) * time.Millisecond
	if interval == 0 {
		interval = time.Millisecond
	}
	ticker := time.NewTicker(interval)
	defer ticker.Stop()
	var statusLines, visibleLines []string
	var horizontalScroll oled.HorizontalScroller
	for {
		now := time.Now()
		lines, err := monitor.Poll(ctx)
		if err != nil {
			if ctx.Err() != nil {
				break
			}
			logger.Warn("could not refresh PiSCSI status; retaining last screen", "error", err)
		} else if !slices.Equal(lines, statusLines) {
			statusLines = slices.Clone(lines)
			visibleLines = slices.Clone(lines)
			horizontalScroll.Reset(len(visibleLines))
			if ipScreensaver != nil {
				ipScreensaver.Reset(now)
			}
			if blankScreensaver != nil {
				blankScreensaver.Reset(now)
			}
		}
		if len(statusLines) > 0 {
			lineCount := *height / 8
			showStatus := true
			if ipScreensaver != nil {
				if active, redraw := ipScreensaver.Update(now, lineCount); active {
					showStatus = false
					if redraw {
						if err := display.Present(renderer.RenderLineAt(statusLines[len(statusLines)-1], ipScreensaver.Row())); err != nil {
							logger.Error("present screensaver", "error", err)
						}
					}
				}
			}
			if blankScreensaver != nil {
				if active, clear := blankScreensaver.Update(now); active {
					showStatus = false
					if clear {
						if err := display.Clear(); err != nil {
							logger.Error("clear screensaver", "error", err)
						}
					}
				}
			}
			if showStatus {
				if err := display.Present(renderer.RenderScrolled(visibleLines, horizontalScroll.Offsets())); err != nil {
					logger.Error("present screen", "error", err)
				}
				widths := make([]int, len(visibleLines))
				for i, line := range visibleLines {
					widths[i] = renderer.TextWidth(line)
				}
				horizontalScroll.Advance(widths, *horizontalScrollStep)
				if len(visibleLines) > lineCount {
					visibleLines = append(visibleLines[1:], visibleLines[0])
					horizontalScroll.Rotate()
				}
			}
		}
		select {
		case <-ctx.Done():
			break
		case <-ticker.C:
		}
		if ctx.Err() != nil {
			break
		}
	}
	shutdown(display, renderer, logger)
}

func shutdown(display oled.Display, renderer *oled.Renderer, logger *slog.Logger) {
	if stop, err := renderer.Splash(false); err != nil {
		logger.Error("render stop splash", "error", err)
	} else if err := display.Present(stop); err != nil {
		logger.Error("show stop splash", "error", err)
	}
	time.Sleep(700 * time.Millisecond)
	if err := display.Clear(); err != nil {
		logger.Error("clear display", "error", err)
	}
}
