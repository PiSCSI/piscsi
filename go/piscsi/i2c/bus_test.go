package i2c

import (
	"sync"
	"testing"
	"time"
)

type fakeTransaction struct{}

func (fakeTransaction) Write([]byte) error             { return nil }
func (fakeTransaction) Read([]byte) error              { return nil }
func (fakeTransaction) WriteRead([]byte, []byte) error { return nil }

func TestInputPriorityRunsBeforeQueuedDisplayWork(t *testing.T) {
	started, release := make(chan struct{}), make(chan struct{})
	var mu sync.Mutex
	var order []int
	bus := newBus(func(address int, fn func(Transaction) error) error { return fn(fakeTransaction{}) }, nil)
	defer bus.Close()
	firstDone := make(chan error, 1)
	go func() {
		firstDone <- bus.Do(Normal, 0x3c, func(Transaction) error {
			close(started)
			<-release
			mu.Lock()
			order = append(order, 1)
			mu.Unlock()
			return nil
		})
	}()
	<-started
	normalDone, inputDone := make(chan error, 1), make(chan error, 1)
	go func() {
		normalDone <- bus.Do(Normal, 0x3c, func(Transaction) error { mu.Lock(); order = append(order, 3); mu.Unlock(); return nil })
	}()
	go func() {
		inputDone <- bus.Do(Input, 0x3f, func(Transaction) error { mu.Lock(); order = append(order, 2); mu.Unlock(); return nil })
	}()
	deadline := time.Now().Add(time.Second)
	for (len(bus.normal) != 1 || len(bus.high) != 1) && time.Now().Before(deadline) {
		time.Sleep(time.Millisecond)
	}
	if len(bus.normal) != 1 || len(bus.high) != 1 {
		t.Fatalf("queued normal/input transactions = %d/%d", len(bus.normal), len(bus.high))
	}
	close(release)
	if err := <-firstDone; err != nil {
		t.Fatal(err)
	}
	if err := <-inputDone; err != nil {
		t.Fatal(err)
	}
	if err := <-normalDone; err != nil {
		t.Fatal(err)
	}
	mu.Lock()
	defer mu.Unlock()
	if len(order) != 3 || order[0] != 1 || order[1] != 2 || order[2] != 3 {
		t.Fatalf("transaction order = %v, want [1 2 3]", order)
	}
}
