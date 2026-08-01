// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

// Package i2c serializes access to a Linux I2C bus. It gives short input
// transactions precedence over the next display transaction without allowing
// either client to interleave bytes for different slave addresses.
package i2c

import (
	"errors"
	"fmt"
	"sync"
	"sync/atomic"
)

var ErrClosed = errors.New("I2C bus is closed")

// Priority controls which queued transaction is selected next. An in-flight
// transaction is never interrupted, so display users must keep work bounded.
type Priority uint8

const (
	Normal Priority = iota
	Input
)

// Transaction operates on a single slave address selected by Bus.Do.
type Transaction interface {
	Write([]byte) error
	Read([]byte) error
	WriteRead(write, read []byte) error
}

type transactionRunner func(address int, fn func(Transaction) error) error

type request struct {
	address int
	fn      func(Transaction) error
	done    chan error
}

// Bus owns one physical I2C device and runs all transactions in one worker.
type Bus struct {
	high, normal chan request
	stop         chan struct{}
	done         chan struct{}
	run          transactionRunner
	closeDevice  func() error
	closed       atomic.Bool
	closeOnce    sync.Once
	closeErr     error
}

func newBus(run transactionRunner, closeDevice func() error) *Bus {
	b := &Bus{
		high: make(chan request, 64), normal: make(chan request, 64),
		stop: make(chan struct{}), done: make(chan struct{}), run: run, closeDevice: closeDevice,
	}
	go b.loop()
	return b
}

// Do selects address and executes fn atomically against other bus users.
func (b *Bus) Do(priority Priority, address int, fn func(Transaction) error) error {
	if b == nil || fn == nil {
		return fmt.Errorf("I2C transaction is required")
	}
	if address < 0 || address > 0x7f {
		return fmt.Errorf("invalid I2C address %#x", address)
	}
	if b.closed.Load() {
		return ErrClosed
	}
	request := request{address: address, fn: fn, done: make(chan error, 1)}
	queue := b.normal
	if priority == Input {
		queue = b.high
	}
	select {
	case queue <- request:
	case <-b.done:
		return ErrClosed
	}
	select {
	case err := <-request.done:
		return err
	case <-b.done:
		return ErrClosed
	}
}

func (b *Bus) loop() {
	defer close(b.done)
	for {
		// Drain already queued input before accepting display work.
		select {
		case request := <-b.high:
			request.done <- b.run(request.address, request.fn)
			continue
		default:
		}
		select {
		case <-b.stop:
			return
		case request := <-b.high:
			request.done <- b.run(request.address, request.fn)
		case request := <-b.normal:
			request.done <- b.run(request.address, request.fn)
		}
	}
}

func (b *Bus) Close() error {
	if b == nil {
		return nil
	}
	b.closeOnce.Do(func() {
		b.closed.Store(true)
		close(b.stop)
		<-b.done
		if b.closeDevice != nil {
			b.closeErr = b.closeDevice()
		}
	})
	return b.closeErr
}
