// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

// Package ctrlboard contains the latency-sensitive Control Board application
// boundary. Hardware callbacks only read an input snapshot and offer semantic
// events to a bounded queue; menu, network, and display work live elsewhere.
package ctrlboard

import (
	"context"
	"fmt"
	"sync/atomic"
	"time"
)

const (
	EncoderAPin      = 0
	EncoderBPin      = 1
	Button1Pin       = 2
	Button2Pin       = 3
	RotaryButtonPin  = 5
	LED1Pin          = 6
	LED2Pin          = 7
	DefaultQueueSize = 64
)

// EventType describes an action understood by the menu layer.
type EventType uint8

const (
	EventRotateClockwise EventType = iota + 1
	EventRotateCounterClockwise
	EventSelect
	EventProfile
	EventShutdown
)

func (t EventType) String() string {
	switch t {
	case EventRotateClockwise:
		return "clockwise"
	case EventRotateCounterClockwise:
		return "counter_clockwise"
	case EventSelect:
		return "select"
	case EventProfile:
		return "profile"
	case EventShutdown:
		return "shutdown"
	default:
		return "unknown"
	}
}

// Event is produced after a PCA9554 input snapshot has been decoded.
type Event struct {
	Type       EventType
	CapturedAt time.Time
}

// EventQueue is intentionally bounded. Dropped input is observable rather
// than being hidden in an unbounded aggregate buffer.
type EventQueue struct {
	events  chan Event
	dropped atomic.Uint64
}

func NewEventQueue(size int) *EventQueue {
	if size <= 0 {
		size = DefaultQueueSize
	}
	return &EventQueue{events: make(chan Event, size)}
}

func (q *EventQueue) Offer(event Event) bool {
	select {
	case q.events <- event:
		return true
	default:
		q.dropped.Add(1)
		return false
	}
}

func (q *EventQueue) Events() <-chan Event { return q.events }
func (q *EventQueue) Dropped() uint64      { return q.dropped.Load() }

// PCA9554 provides the sole hardware operation permitted on the input path.
type PCA9554 interface{ ReadInput() (byte, error) }

// Decoder converts active-low PCA9554 snapshots into semantic events.
// A full quadrature detent emits one event; invalid transitions reset the
// partial detent so contact bounce cannot create a reversed step.
type Decoder struct {
	encoderKnown bool
	encoderState byte
	encoderSteps int
	buttonAt     [8]time.Time
	debounce     time.Duration
}

func NewDecoder(debounce time.Duration) *Decoder {
	if debounce < 0 {
		debounce = 0
	}
	return &Decoder{debounce: debounce}
}

// Prime establishes the current idle hardware state before GPIO events are
// accepted. This avoids treating the first partial encoder movement as a full
// detent.
func (d *Decoder) Prime(snapshot byte) {
	d.encoderKnown = true
	d.encoderState = snapshot & 0x03
	d.encoderSteps = 0
}

// Decode processes one complete input register snapshot. On the PiSCSI
// Control Board, a physical clockwise detent is 11 -> 01 -> 00 -> 10 -> 11;
// this mapping is verified by the hardware diagnostic trace.
func (d *Decoder) Decode(snapshot byte, at time.Time) []Event {
	events := make([]Event, 0, 2)
	state := snapshot & 0x03
	if !d.encoderKnown {
		d.encoderKnown, d.encoderState = true, state
	} else if state != d.encoderState {
		if delta, valid := quadratureDelta(d.encoderState, state); valid {
			d.encoderSteps += delta
			if d.encoderSteps == 4 {
				events = append(events, Event{Type: EventRotateClockwise, CapturedAt: at})
				d.encoderSteps = 0
			} else if d.encoderSteps == -4 {
				events = append(events, Event{Type: EventRotateCounterClockwise, CapturedAt: at})
				d.encoderSteps = 0
			}
		} else {
			d.encoderSteps = 0
		}
		d.encoderState = state
	}
	for _, button := range []struct {
		pin   int
		event EventType
	}{
		{RotaryButtonPin, EventSelect}, {Button1Pin, EventProfile}, {Button2Pin, EventShutdown},
	} {
		// The PCA9554 can coalesce the release transition with nearby input
		// changes, so a high snapshot is not a reliable rearm signal. Match
		// the legacy Control Board behavior: accept active-low input again
		// after its debounce interval. This also supports the two held-button
		// cycling workflows.
		if snapshot&(1<<uint(button.pin)) == 0 && (d.buttonAt[button.pin].IsZero() || at.Sub(d.buttonAt[button.pin]) >= d.debounce) {
			d.buttonAt[button.pin] = at
			events = append(events, Event{Type: button.event, CapturedAt: at})
		}
	}
	return events
}

func quadratureDelta(previous, current byte) (int, bool) {
	switch previous<<2 | current {
	case 0b0010, 0b1011, 0b1101, 0b0100:
		return 1, true
	case 0b0001, 0b0111, 0b1110, 0b1000:
		return -1, true
	default:
		return 0, false
	}
}

// InputReader is called from the GPIO edge path. It performs one I2C read,
// decodes that snapshot, and never blocks on menu or rendering work.
type InputReader struct {
	pca        PCA9554
	decoder    *Decoder
	queue      *EventQueue
	diagnostic chan<- InputSnapshot
	edges      atomic.Uint64
	snapshots  atomic.Uint64
	semantic   atomic.Uint64
	errors     atomic.Uint64
	traceDrops atomic.Uint64
}

func NewInputReader(pca PCA9554, decoder *Decoder, queue *EventQueue) *InputReader {
	return &InputReader{pca: pca, decoder: decoder, queue: queue}
}

// InputSnapshot is an optional, best-effort diagnostic record. It is sent to
// a bounded channel; producing it never performs logging or blocks the input
// path.
type InputSnapshot struct {
	At        time.Time
	Value     byte
	Events    []Event
	ReadError error
}

// InputStats exposes input-path counters for periodic diagnostics.
type InputStats struct {
	EdgeReceipts    uint64
	Snapshots       uint64
	SemanticEvents  uint64
	ReadErrors      uint64
	DiagnosticDrops uint64
}

// SetDiagnosticSink enables non-blocking trace records. Call it before the
// input loop begins; normal operation should leave it unset.
func (r *InputReader) SetDiagnosticSink(sink chan<- InputSnapshot) { r.diagnostic = sink }

// Prime reads and establishes the initial PCA9554 state before subscribing to
// user input. It is not treated as a GPIO edge or a semantic input event.
func (r *InputReader) Prime(at time.Time) error {
	snapshot, err := r.pca.ReadInput()
	if err != nil {
		r.errors.Add(1)
		r.trace(InputSnapshot{At: at, ReadError: err})
		return err
	}
	r.snapshots.Add(1)
	r.decoder.Prime(snapshot)
	r.trace(InputSnapshot{At: at, Value: snapshot})
	return nil
}

func (r *InputReader) HandleEdge(at time.Time) {
	r.edges.Add(1)
	snapshot, err := r.pca.ReadInput()
	if err != nil {
		r.errors.Add(1)
		r.trace(InputSnapshot{At: at, ReadError: err})
		return
	}
	r.snapshots.Add(1)
	events := r.decoder.Decode(snapshot, at)
	r.semantic.Add(uint64(len(events)))
	for _, event := range events {
		r.queue.Offer(event)
	}
	r.trace(InputSnapshot{At: at, Value: snapshot, Events: events})
}

func (r *InputReader) ReadErrors() uint64 { return r.errors.Load() }

func (r *InputReader) Stats() InputStats {
	return InputStats{
		EdgeReceipts: r.edges.Load(), Snapshots: r.snapshots.Load(), SemanticEvents: r.semantic.Load(),
		ReadErrors: r.errors.Load(), DiagnosticDrops: r.traceDrops.Load(),
	}
}

func (r *InputReader) trace(snapshot InputSnapshot) {
	if r.diagnostic == nil {
		return
	}
	select {
	case r.diagnostic <- snapshot:
	default:
		r.traceDrops.Add(1)
	}
}

// EdgeSource represents a GPIO falling-edge subscription. The Linux
// implementation will call InputReader.HandleEdge directly for every event.
type EdgeSource interface {
	Run(context.Context, func(time.Time)) error
}

func RunInput(ctx context.Context, source EdgeSource, reader *InputReader) error {
	if source == nil || reader == nil {
		return fmt.Errorf("input source and reader are required")
	}
	return source.Run(ctx, reader.HandleEdge)
}
