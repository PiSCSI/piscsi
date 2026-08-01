// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

// piscsi-ctrlboard is the Go Control Board service. This first delivery
// intentionally exposes diagnostic input instrumentation before it becomes
// the installer default on physical hardware.
package main

import (
	"context"
	"flag"
	"fmt"
	"log/slog"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/piscsi/piscsi/go/piscsi"
	ctrlboard "github.com/piscsi/piscsi/go/piscsi-ctrlboard"
	oled "github.com/piscsi/piscsi/go/piscsi-oled"
	"github.com/piscsi/piscsi/go/piscsi/i2c"
)

func main() {
	diagnostic := flag.Bool("diagnostic", false, "log input latency counters")
	debounce := flag.Duration("button-debounce", 200*time.Millisecond, "button debounce interval")
	queueSize := flag.Int("event-queue-size", ctrlboard.DefaultQueueSize, "bounded hardware event queue size")
	chip := flag.String("gpiochip", "/dev/gpiochip0", "Linux GPIO character device")
	interruptLine := flag.Int("interrupt-gpio", 9, "BCM GPIO line wired to the PCA9554 interrupt")
	i2cDevice := flag.String("i2c-device", "/dev/i2c-1", "Linux I2C device")
	pcaAddress := flag.Int("pca-address", 0x3f, "PCA9554 I2C address")
	displayEnabled := flag.Bool("display", true, "render the Control Board menu on the SSD1306 display")
	displayAddress := flag.Int("display-address", 0x3c, "SSD1306 I2C address")
	rotation := flag.Int("rotation", 0, "screen rotation in degrees (0 or 180)")
	transitions := flag.Bool("transitions", false, "animate submenu navigation with horizontal push transitions")
	splashDuration := flag.Duration("splash-duration", 2*time.Second, "startup splash duration (0 disables the delay)")
	shutdownSplashDuration := flag.Duration("shutdown-splash-duration", 700*time.Millisecond, "shutdown splash duration (0 disables the delay)")
	screensaverTimeout := flag.Duration("screensaver-timeout", 10*time.Minute, "idle timeout before the IP screensaver (0 disables it)")
	screensaverMoveInterval := flag.Duration("screensaver-move-interval", 30*time.Second, "IP screensaver move interval")
	refreshInterval := flag.Duration("refresh-interval", 10*time.Second, "PiSCSI root-menu refresh interval")
	buttonCycleTimeout := flag.Duration("button-cycle-timeout", 3*time.Second, "inactivity timeout before an auxiliary-button choice runs")
	password := flag.String("password", "", "token password for authenticating with the PiSCSI daemon")
	host := flag.String("host", piscsi.DefaultHost, "PiSCSI daemon hostname")
	port := flag.Int("port", piscsi.DefaultPort, "PiSCSI daemon port")
	configDir := flag.String("config-dir", "/var/lib/piscsi/config", "object-style PiSCSI profile directory")
	imageDir := flag.String("image-dir", "/var/lib/piscsi/images", "image directory used when loading profiles")
	flag.Parse()
	client := piscsi.NewClient(*host, *port)

	logger := slog.New(slog.NewTextHandler(os.Stdout, nil))
	if *queueSize <= 0 || *debounce < 0 || *refreshInterval <= 0 || *buttonCycleTimeout <= 0 || *splashDuration < 0 || *shutdownSplashDuration < 0 || *screensaverTimeout < 0 || *screensaverMoveInterval <= 0 {
		logger.Error("invalid input configuration")
		os.Exit(2)
	}
	if *rotation != 0 && *rotation != 180 {
		logger.Error("rotation must be 0 or 180")
		os.Exit(2)
	}
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()
	bus, err := i2c.Open(*i2cDevice)
	if err != nil {
		logger.Error("open Control Board I2C bus", "error", err)
		os.Exit(1)
	}
	defer bus.Close()
	queue := ctrlboard.NewEventQueue(*queueSize)
	var reader *ctrlboard.InputReader
	pca, err := ctrlboard.OpenPCA9554WithBus(bus, *pcaAddress)
	if err != nil {
		logger.Error("open control board expander", "error", err)
		os.Exit(1)
	}
	defer pca.Close()
	if err := pca.ConfigurePins(0x2f); err != nil {
		logger.Error("configure control board expander", "error", err)
		os.Exit(1)
	}
	reader = ctrlboard.NewInputReader(pca, ctrlboard.NewDecoder(*debounce), queue)
	var traces chan ctrlboard.InputSnapshot
	if *diagnostic {
		traces = make(chan ctrlboard.InputSnapshot, 256)
		reader.SetDiagnosticSink(traces)
	}
	if err := reader.Prime(time.Now()); err != nil {
		logger.Error("read initial control board state", "error", err)
		os.Exit(1)
	}
	var menuController *ctrlboard.MenuController
	var renderer *ctrlboard.Renderer
	var buttonCycler *ctrlboard.ButtonCycler
	var controlDisplay oled.Display
	if *displayEnabled {
		renderer, err = ctrlboard.NewRenderer(*rotation)
		if err != nil {
			logger.Error("create Control Board renderer", "error", err)
			os.Exit(1)
		}
		defer renderer.Close()
		display, err := oled.NewSSD1306WithBus(oled.SSD1306Config{
			Device: *i2cDevice, Address: *displayAddress, Height: ctrlboard.DisplayHeight,
			// Renderer applies rotation so tests cover the exact pixels presented.
			Rotation: 0,
		}, bus)
		if err != nil {
			logger.Error("create Control Board display", "error", err)
			os.Exit(1)
		}
		if err := display.Init(); err != nil {
			logger.Error("initialize Control Board display", "error", err)
			os.Exit(1)
		}
		defer display.Close()
		controlDisplay = display
		if splash, err := renderer.Splash(true); err != nil {
			logger.Warn("render Control Board startup splash", "error", err)
		} else if err := display.Present(splash); err != nil {
			logger.Warn("show Control Board startup splash", "error", err)
		} else if *splashDuration > 0 {
			select {
			case <-ctx.Done():
				return
			case <-time.After(*splashDuration):
			}
		}
		menuController, err = ctrlboard.NewMenuController(initialMenu(renderer.Rows()), renderer, display)
		if err != nil {
			logger.Error("create Control Board menu", "error", err)
			os.Exit(1)
		}
		menuController.SetTransitions(*transitions)
		workflow := ctrlboard.NewSCSIWorkflow(client, *password)
		workflow.SetProfileDirectories(*configDir, *imageDir)
		if *diagnostic {
			workflow.SetDiagnosticSink(func(response ctrlboard.ImageListDiagnostic) {
				logger.Info("control board image list", "status", response.Status, "message", response.Message, "folder", response.Folder, "count", response.Count)
			})
		}
		workflowController, err := ctrlboard.NewWorkflowController(ctx, menuController, workflow, renderer.Rows(), func(err error) {
			logger.Warn("Control Board workflow", "error", err)
		})
		if err != nil {
			logger.Error("create Control Board workflow", "error", err)
			os.Exit(1)
		}
		menuController.SetSelectHandler(workflowController.Handle)
		if *screensaverTimeout > 0 {
			saver, err := ctrlboard.NewIPScreenSaver(*screensaverTimeout, *screensaverMoveInterval)
			if err != nil {
				logger.Error("create Control Board screensaver", "error", err)
				os.Exit(1)
			}
			menuController.SetScreenSaver(saver)
		}
		buttonCycler, err = ctrlboard.NewButtonCycler(ctx, workflow, menuController, *buttonCycleTimeout, func(err error) {
			logger.Warn("Control Board button workflow", "error", err)
		})
		if err != nil {
			logger.Error("create Control Board button workflow", "error", err)
			os.Exit(1)
		}
	}
	edges, err := ctrlboard.OpenFallingEdgeSource(*chip, *interruptLine)
	if err != nil {
		logger.Error("subscribe to control board interrupt", "error", err)
		os.Exit(1)
	}
	defer edges.Close()

	renderDone := make(chan error, 1)
	if menuController != nil {
		menuController.RequestRedraw()
		go func() {
			err := menuController.Run(ctx)
			renderDone <- err
			if err != nil {
				stop()
			}
		}()
		refresher, err := ctrlboard.NewSCSIRefresher(ctrlboard.NewSCSIMenuBuilder(client, *password), menuController, *refreshInterval, func(err error) {
			logger.Warn("refresh Control Board SCSI menu", "error", err)
		})
		if err != nil {
			logger.Error("create Control Board SCSI refresher", "error", err)
			stop()
			os.Exit(1)
		}
		menuController.SetRootRefresh(refresher.RequestRefresh)
		go refresher.Run(ctx)
	}
	if traces != nil {
		go logInputTraces(ctx, logger, traces)
	}
	inputDone := make(chan error, 1)
	go func() {
		err := ctrlboard.RunInput(ctx, edges, reader)
		inputDone <- err
		if err != nil {
			stop()
		}
	}()
	app, err := ctrlboard.NewApp(queue, func(_ context.Context, event ctrlboard.Event) error {
		if buttonCycler != nil && buttonCycler.Handle(event) {
			return nil
		}
		if menuController != nil {
			menuController.Handle(event)
		}
		if *diagnostic {
			logger.Debug("control board input", "event", event.Type)
		}
		return nil
	}, *diagnostic)
	if err != nil {
		logger.Error("create control board application", "error", err)
		stop()
		os.Exit(1)
	}
	if *diagnostic {
		go func() {
			ticker := time.NewTicker(10 * time.Second)
			defer ticker.Stop()
			for {
				select {
				case <-ctx.Done():
					return
				case <-ticker.C:
					latency := app.Latency()
					input := reader.Stats()
					logger.Info("control board input diagnostics", "updates", latency.Updates, "average", latency.Average, "maximum", latency.Maximum, "dropped_events", queue.Dropped(), "gpio_edges", input.EdgeReceipts, "pca_snapshots", input.Snapshots, "semantic_events", input.SemanticEvents, "pca_read_errors", input.ReadErrors, "trace_drops", input.DiagnosticDrops)
				}
			}
		}()
	}
	appErr := app.Run(ctx)
	inputErr := <-inputDone
	var renderErr error
	if menuController != nil {
		renderErr = <-renderDone
	}
	if appErr != nil {
		logger.Error("control board stopped", "error", appErr)
		os.Exit(1)
	}
	if inputErr != nil {
		logger.Error("control board input stopped", "error", inputErr)
		os.Exit(1)
	}
	if renderErr != nil {
		logger.Error("Control Board renderer stopped", "error", renderErr)
		os.Exit(1)
	}
	if controlDisplay != nil && renderer != nil {
		if splash, err := renderer.Splash(false); err != nil {
			logger.Warn("render Control Board shutdown splash", "error", err)
		} else if err := controlDisplay.Present(splash); err != nil {
			logger.Warn("show Control Board shutdown splash", "error", err)
		} else if *shutdownSplashDuration > 0 {
			time.Sleep(*shutdownSplashDuration)
		}
	}
}

func initialMenu(rows int) *ctrlboard.Menu {
	items := make([]ctrlboard.MenuItem, 8)
	for id := range items {
		items[id] = ctrlboard.MenuItem{ID: fmt.Sprintf("scsi-%d", id), Label: fmt.Sprintf("%d: (loading...)", id)}
	}
	menu, err := ctrlboard.NewMenu("SCSI IDs", items, rows)
	if err != nil {
		panic(err)
	}
	return menu
}

func logInputTraces(ctx context.Context, logger *slog.Logger, traces <-chan ctrlboard.InputSnapshot) {
	for {
		select {
		case <-ctx.Done():
			return
		case trace := <-traces:
			if trace.ReadError != nil {
				logger.Warn("control board PCA9554 read failed", "error", trace.ReadError)
				continue
			}
			events := make([]string, len(trace.Events))
			for index, event := range trace.Events {
				events[index] = event.Type.String()
			}
			logger.Info("control board input snapshot", "input", fmt.Sprintf("0x%02x", trace.Value), "events", events)
		}
	}
}
