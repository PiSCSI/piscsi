// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"context"
	"fmt"
	"sync"
	"time"
)

const defaultButtonCycleTimeout = 3 * time.Second

type buttonCycleChoice struct {
	label   string
	profile *ProfileSelection
	power   *SystemCommandSelection
}

type buttonCycle struct {
	choices    []buttonCycleChoice
	index      int
	generation uint64
	loading    bool
}

// ButtonCycler implements the two timed auxiliary-button workflows. Button
// handlers only update in-memory state and schedule timers; filesystem and
// daemon operations run in separate goroutines after the choice settles.
type ButtonCycler struct {
	ctx      context.Context
	workflow *SCSIWorkflow
	menu     *MenuController
	timeout  time.Duration
	onError  func(error)

	mu       sync.Mutex
	profiles *buttonCycle
	power    *buttonCycle
}

func NewButtonCycler(ctx context.Context, workflow *SCSIWorkflow, menu *MenuController, timeout time.Duration, onError func(error)) (*ButtonCycler, error) {
	if ctx == nil || workflow == nil || menu == nil {
		return nil, fmt.Errorf("context, workflow, and menu are required")
	}
	if timeout <= 0 {
		return nil, fmt.Errorf("button cycle timeout must be positive")
	}
	return &ButtonCycler{ctx: ctx, workflow: workflow, menu: menu, timeout: timeout, onError: onError}, nil
}

// Handle consumes profile and shutdown button events. It returns false for
// normal rotary navigation events, which the MenuController should handle.
func (c *ButtonCycler) Handle(event Event) bool {
	switch event.Type {
	case EventProfile:
		c.menu.MarkActivity()
		c.cycleProfile()
		return true
	case EventShutdown:
		c.menu.MarkActivity()
		c.cyclePower()
		return true
	default:
		return false
	}
}

func (c *ButtonCycler) cycleProfile() {
	c.mu.Lock()
	if c.profiles != nil {
		if c.profiles.loading {
			c.mu.Unlock()
			return
		}
		c.advanceLocked(c.profiles, c.executeProfile)
		c.mu.Unlock()
		return
	}
	c.profiles = &buttonCycle{loading: true}
	c.mu.Unlock()
	c.menu.ShowMessage("Loading profiles", c.timeout)
	go func() {
		menu, err := c.workflow.BuildProfileMenu(1)
		if err != nil {
			c.report(err)
			c.menu.ShowMessage("Profiles unavailable", 1500*time.Millisecond)
			c.mu.Lock()
			c.profiles = nil
			c.mu.Unlock()
			return
		}
		choices := []buttonCycleChoice{{label: "Return ->"}}
		for _, item := range menu.Items {
			if profile, ok := item.Data.(ProfileSelection); ok {
				profile := profile
				choices = append(choices, buttonCycleChoice{label: profile.Filename, profile: &profile})
			}
		}
		c.mu.Lock()
		if c.profiles == nil || !c.profiles.loading {
			c.mu.Unlock()
			return
		}
		c.profiles.choices, c.profiles.loading = choices, false
		c.advanceLocked(c.profiles, c.executeProfile)
		c.mu.Unlock()
	}()
}

func (c *ButtonCycler) cyclePower() {
	c.mu.Lock()
	if c.power == nil {
		shutdown := SystemCommandSelection{Kind: SystemCommandShutdown}
		reboot := SystemCommandSelection{Kind: SystemCommandReboot}
		c.power = &buttonCycle{choices: []buttonCycleChoice{
			{label: "Return ->"}, {label: "Shutdown", power: &shutdown}, {label: "Reboot", power: &reboot},
		}}
		c.advanceLocked(c.power, c.executePower)
		c.mu.Unlock()
		return
	}
	c.advanceLocked(c.power, c.executePower)
	c.mu.Unlock()
}

func (c *ButtonCycler) advanceLocked(cycle *buttonCycle, execute func(uint64)) {
	if len(cycle.choices) == 0 {
		return
	}
	if cycle.generation > 0 {
		cycle.index = (cycle.index + 1) % len(cycle.choices)
	}
	cycle.generation++
	generation := cycle.generation
	c.menu.ShowMessage(cycle.choices[cycle.index].label, c.timeout)
	time.AfterFunc(c.timeout, func() { execute(generation) })
}

func (c *ButtonCycler) executeProfile(generation uint64) {
	c.mu.Lock()
	if c.profiles == nil || c.profiles.generation != generation || c.profiles.loading {
		c.mu.Unlock()
		return
	}
	choice := c.profiles.choices[c.profiles.index]
	c.profiles = nil
	c.mu.Unlock()
	if choice.profile == nil {
		c.menu.ResetToRoot()
		c.menu.ClearMessage()
		return
	}
	go func() {
		message, err := c.workflow.LoadProfile(c.ctx, *choice.profile)
		if err != nil {
			c.report(err)
			c.menu.ShowMessage("Profile failed", 1500*time.Millisecond)
			return
		}
		c.menu.ResetToRoot()
		c.menu.RequestRootRefresh()
		c.menu.ShowMessage(message, 1500*time.Millisecond)
	}()
}

func (c *ButtonCycler) executePower(generation uint64) {
	c.mu.Lock()
	if c.power == nil || c.power.generation != generation {
		c.mu.Unlock()
		return
	}
	choice := c.power.choices[c.power.index]
	c.power = nil
	c.mu.Unlock()
	if choice.power == nil {
		c.menu.ClearMessage()
		return
	}
	go func() {
		message, err := c.workflow.RunSystemCommand(c.ctx, *choice.power)
		if err != nil {
			c.report(err)
			c.menu.ShowMessage("Action failed", 1500*time.Millisecond)
			return
		}
		c.menu.ResetToRoot()
		c.menu.ShowMessage(message, 1500*time.Millisecond)
	}()
}

func (c *ButtonCycler) report(err error) {
	if err != nil && c.onError != nil {
		c.onError(err)
	}
}
