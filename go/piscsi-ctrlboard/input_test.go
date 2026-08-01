package ctrlboard

import (
	"errors"
	"testing"
	"time"
)

func TestDecoderQuadratureAndBounce(t *testing.T) {
	d := NewDecoder(200 * time.Millisecond)
	now := time.Now()
	var got []Event
	for _, state := range []byte{0b11, 0b01, 0b00, 0b10, 0b11} {
		got = append(got, d.Decode(state|0b11111100, now)...)
		now = now.Add(time.Millisecond)
	}
	if len(got) != 1 || got[0].Type != EventRotateClockwise {
		t.Fatalf("clockwise events = %#v", got)
	}
	if events := d.Decode(0b11111011, now); len(events) != 1 || events[0].Type != EventProfile {
		t.Fatalf("button event = %#v", events)
	}
	if events := d.Decode(0b11111011, now.Add(time.Millisecond)); len(events) != 0 {
		t.Fatalf("bounce events = %#v", events)
	}
	if events := d.Decode(0b11111011, now.Add(time.Second)); len(events) != 1 || events[0].Type != EventProfile {
		t.Fatalf("debounced repeated button event = %#v", events)
	}
	if events := d.Decode(0b11111011, now.Add(time.Second+100*time.Millisecond)); len(events) != 0 {
		t.Fatalf("second bounce events = %#v", events)
	}
	if events := d.Decode(0b11111011, now.Add(2*time.Second)); len(events) != 1 {
		t.Fatalf("coalesced-release button event = %#v", events)
	}
}

func TestEventQueueDropsExplicitly(t *testing.T) {
	q := NewEventQueue(1)
	if !q.Offer(Event{}) || q.Offer(Event{}) || q.Dropped() != 1 {
		t.Fatalf("queue did not report a dropped event")
	}
}

type fakePCA struct {
	snapshots []byte
	err       error
}

func (p *fakePCA) ReadInput() (byte, error) {
	if p.err != nil {
		return 0, p.err
	}
	value := p.snapshots[0]
	p.snapshots = p.snapshots[1:]
	return value, nil
}

func TestInputReaderReportsReadErrors(t *testing.T) {
	reader := NewInputReader(&fakePCA{err: errors.New("unavailable")}, NewDecoder(0), NewEventQueue(1))
	reader.HandleEdge(time.Now())
	if reader.ReadErrors() != 1 {
		t.Fatalf("read errors = %d", reader.ReadErrors())
	}
}

func TestInputReaderPrimesAndTracesWithoutBlocking(t *testing.T) {
	reader := NewInputReader(&fakePCA{snapshots: []byte{0xff, 0xfe, 0xfc, 0xfd, 0xff}}, NewDecoder(0), NewEventQueue(2))
	trace := make(chan InputSnapshot, 5)
	reader.SetDiagnosticSink(trace)
	now := time.Now()
	if err := reader.Prime(now); err != nil {
		t.Fatal(err)
	}
	for range 4 {
		now = now.Add(time.Millisecond)
		reader.HandleEdge(now)
	}
	stats := reader.Stats()
	if stats.EdgeReceipts != 4 || stats.Snapshots != 5 || stats.SemanticEvents != 1 {
		t.Fatalf("stats = %+v", stats)
	}
	if got := len(trace); got != 5 {
		t.Fatalf("trace records = %d, want 5", got)
	}
}
