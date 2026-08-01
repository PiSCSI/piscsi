package ctrlboard

import (
	"context"
	"testing"
	"time"
)

func TestAppRecordsLatency(t *testing.T) {
	q := NewEventQueue(1)
	app, err := NewApp(q, func(context.Context, Event) error { return nil }, true)
	if err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan error, 1)
	go func() { done <- app.Run(ctx) }()
	q.Offer(Event{Type: EventSelect, CapturedAt: time.Now().Add(-time.Millisecond)})
	deadline := time.Now().Add(time.Second)
	for app.Latency().Updates == 0 && time.Now().Before(deadline) {
		time.Sleep(time.Millisecond)
	}
	cancel()
	if err := <-done; err != nil {
		t.Fatal(err)
	}
	if snapshot := app.Latency(); snapshot.Updates != 1 || snapshot.Maximum <= 0 {
		t.Fatalf("latency = %+v", snapshot)
	}
}
