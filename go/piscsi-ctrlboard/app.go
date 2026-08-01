// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"context"
	"fmt"
	"sync/atomic"
	"time"
)

// EventHandler mutates menu state. It must schedule expensive refresh and
// rendering work asynchronously rather than doing it in an input callback.
type EventHandler func(context.Context, Event) error

// Latency reports input-path timing only when diagnostics are enabled.
type Latency struct {
	updates atomic.Uint64
	totalNS atomic.Uint64
	maxNS   atomic.Uint64
}

func (l *Latency) Record(duration time.Duration) {
	if duration < 0 {
		return
	}
	ns := uint64(duration)
	l.updates.Add(1)
	l.totalNS.Add(ns)
	for current := l.maxNS.Load(); ns > current && !l.maxNS.CompareAndSwap(current, ns); current = l.maxNS.Load() {
	}
}

type LatencySnapshot struct {
	Updates          uint64
	Average, Maximum time.Duration
}

func (l *Latency) Snapshot() LatencySnapshot {
	updates := l.updates.Load()
	if updates == 0 {
		return LatencySnapshot{}
	}
	return LatencySnapshot{Updates: updates, Average: time.Duration(l.totalNS.Load() / updates), Maximum: time.Duration(l.maxNS.Load())}
}

// App drains input independently from hardware collection and records the
// edge-to-menu-update latency in diagnostic mode.
type App struct {
	queue       *EventQueue
	handle      EventHandler
	diagnostics bool
	latency     Latency
}

func NewApp(queue *EventQueue, handler EventHandler, diagnostics bool) (*App, error) {
	if queue == nil || handler == nil {
		return nil, fmt.Errorf("event queue and handler are required")
	}
	return &App{queue: queue, handle: handler, diagnostics: diagnostics}, nil
}

func (a *App) Run(ctx context.Context) error {
	for {
		select {
		case <-ctx.Done():
			return nil
		case event := <-a.queue.Events():
			if err := a.handle(ctx, event); err != nil {
				return err
			}
			if a.diagnostics {
				a.latency.Record(time.Since(event.CapturedAt))
			}
		}
	}
}

func (a *App) Latency() LatencySnapshot { return a.latency.Snapshot() }
