// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"context"
	"fmt"
	"sync"
	"time"

	oled "github.com/piscsi/piscsi/go/piscsi-oled"
)

const (
	scrollStartDelay    = time.Second
	scrollInterval      = 75 * time.Millisecond
	transitionInterval  = 50 * time.Millisecond
	transitionFrameStep = 4
)

type menuTransition struct {
	from, to *Menu
	left     bool
	step     int
}

// FramePresenter is satisfied by oled.Display and narrow test fakes.
type FramePresenter interface{ Present(oled.Frame) error }

// MenuController separates state mutation from rendering. Handle is called by
// the application event loop and only queues a redraw; Run performs rendering
// and display writes in its own goroutine.
type MenuController struct {
	mu          sync.Mutex
	root        *Menu
	menu        *Menu
	history     []*Menu
	onSelect    func(MenuItem)
	renderer    *Renderer
	present     FramePresenter
	redraw      chan struct{}
	message     string
	until       time.Time
	scroll      int
	lastMove    time.Time
	saver       *IPScreenSaver
	saveRow     int
	refresh     func()
	transitions bool
	transition  *menuTransition
}

func NewMenuController(menu *Menu, renderer *Renderer, present FramePresenter) (*MenuController, error) {
	if menu == nil || renderer == nil || present == nil {
		return nil, fmt.Errorf("menu, renderer, and presenter are required")
	}
	return &MenuController{root: menu, menu: menu, renderer: renderer, present: present, redraw: make(chan struct{}, 1), lastMove: time.Now()}, nil
}

// Handle applies navigation input without waiting for a display transaction.
// Select is intentionally left to workflow-specific menu actions.
func (c *MenuController) Handle(event Event) bool {
	c.mu.Lock()
	c.resetSaverLocked(time.Now())
	hadTransition := c.transition != nil
	c.transition = nil
	changed := false
	var selected MenuItem
	var selectHandler func(MenuItem)
	switch event.Type {
	case EventRotateClockwise:
		changed = c.menu.Move(1)
	case EventRotateCounterClockwise:
		changed = c.menu.Move(-1)
	case EventSelect:
		selected, changed = c.menu.Selected()
		selectHandler = c.onSelect
	}
	if changed {
		c.resetScrollLocked(time.Now())
	}
	c.mu.Unlock()
	if changed || hadTransition {
		c.requestRedraw()
	}
	if event.Type == EventSelect && selectHandler != nil {
		selectHandler(selected)
	}
	return changed
}

// SetScreenSaver enables the optional idle IP screensaver. Passing nil
// disables it. It is safe to call before Run starts.
func (c *MenuController) SetScreenSaver(saver *IPScreenSaver) {
	c.mu.Lock()
	c.saver = saver
	c.saveRow = -1
	if saver != nil {
		saver.Reset(time.Now())
	}
	c.mu.Unlock()
}

// SetTransitions enables or disables the short horizontal push animation used
// when entering and returning from submenus. It is disabled by default because
// a full SSD1306 frame can be slow on low-speed I2C buses.
func (c *MenuController) SetTransitions(enabled bool) {
	c.mu.Lock()
	hadTransition := c.transition != nil
	c.transitions = enabled
	if !enabled {
		c.transition = nil
	}
	c.mu.Unlock()
	if hadTransition && !enabled {
		c.requestRedraw()
	}
}

// MarkActivity restores the normal menu after input handled outside Handle,
// such as either of the two dedicated cycling buttons.
func (c *MenuController) MarkActivity() {
	c.mu.Lock()
	wasActive := c.saveRow >= 0
	hadTransition := c.transition != nil
	c.transition = nil
	c.resetSaverLocked(time.Now())
	c.mu.Unlock()
	if wasActive || hadTransition {
		c.requestRedraw()
	}
}

// SetRootRefresh installs the coalescing daemon refresher owned by the
// application. Keeping this callback at the controller boundary lets every
// workflow request a fresh SCSI root without knowing about polling details.
func (c *MenuController) SetRootRefresh(refresh func()) {
	c.mu.Lock()
	c.refresh = refresh
	c.mu.Unlock()
}

// RequestRootRefresh schedules an asynchronous root refresh, if configured.
func (c *MenuController) RequestRootRefresh() {
	c.mu.Lock()
	refresh := c.refresh
	c.mu.Unlock()
	if refresh != nil {
		refresh()
	}
}

// SetSelectHandler installs a handler invoked after a rotary select event.
// It runs outside the controller lock and must not perform daemon work inline.
func (c *MenuController) SetSelectHandler(handler func(MenuItem)) {
	c.mu.Lock()
	c.onSelect = handler
	c.mu.Unlock()
}

// Push enters a submenu while retaining the parent for Return navigation.
func (c *MenuController) Push(menu *Menu) error {
	if menu == nil {
		return fmt.Errorf("submenu is required")
	}
	c.mu.Lock()
	from := cloneMenu(c.menu)
	c.history = append(c.history, c.menu)
	c.menu = menu
	c.startTransitionLocked(from, c.menu, true)
	c.resetScrollLocked(time.Now())
	c.resetSaverLocked(time.Now())
	c.mu.Unlock()
	c.requestRedraw()
	return nil
}

// Pop returns to the immediate parent menu.
func (c *MenuController) Pop() bool {
	c.mu.Lock()
	if len(c.history) == 0 {
		c.mu.Unlock()
		return false
	}
	from := cloneMenu(c.menu)
	c.menu = c.history[len(c.history)-1]
	c.history = c.history[:len(c.history)-1]
	c.startTransitionLocked(from, c.menu, false)
	c.resetScrollLocked(time.Now())
	c.resetSaverLocked(time.Now())
	c.mu.Unlock()
	c.requestRedraw()
	return true
}

// ResetToRoot closes every submenu. Workflows use it after a successful
// daemon action so the user sees the refreshed SCSI list rather than a stale
// action or image-selection menu.
func (c *MenuController) ResetToRoot() {
	c.mu.Lock()
	c.menu = c.root
	c.history = nil
	c.resetScrollLocked(time.Now())
	c.resetSaverLocked(time.Now())
	c.mu.Unlock()
	c.RequestRedraw()
}

// ShowMessage temporarily replaces the menu with a centred status message.
// It only mutates controller state; display I/O remains in Run.
func (c *MenuController) ShowMessage(message string, duration time.Duration) {
	if duration <= 0 {
		duration = 1500 * time.Millisecond
	}
	c.mu.Lock()
	c.message = message
	c.until = time.Now().Add(duration)
	c.resetSaverLocked(time.Now())
	c.mu.Unlock()
	c.RequestRedraw()
}

// ClearMessage restores the live menu immediately. It is used when an
// asynchronous navigation operation has completed successfully.
func (c *MenuController) ClearMessage() {
	c.mu.Lock()
	c.message = ""
	c.until = time.Time{}
	c.resetSaverLocked(time.Now())
	c.mu.Unlock()
	c.requestRedraw()
}

// RequestRedraw coalesces render requests, avoiding display-write backlogs
// while rotary input continues to be collected.
func (c *MenuController) RequestRedraw() {
	c.mu.Lock()
	c.transition = nil
	c.mu.Unlock()
	c.requestRedraw()
}

func (c *MenuController) requestRedraw() {
	select {
	case c.redraw <- struct{}{}:
	default:
	}
}

// Snapshot returns a renderer-safe copy of current menu state.
func (c *MenuController) Snapshot() *Menu {
	c.mu.Lock()
	defer c.mu.Unlock()
	copy := *c.menu
	copy.Items = append([]MenuItem(nil), c.menu.Items...)
	return &copy
}

// ReplaceItems updates a refreshed menu model and schedules one redraw. It is
// safe to call from a daemon-refresh goroutine.
func (c *MenuController) ReplaceItems(items []MenuItem) {
	c.mu.Lock()
	c.menu.ReplaceItems(items)
	c.resetScrollLocked(time.Now())
	c.resetSaverLocked(time.Now())
	c.mu.Unlock()
	c.RequestRedraw()
}

// ReplaceRootItems refreshes only the live SCSI root. A background daemon
// refresh therefore cannot replace an action submenu the user is viewing.
func (c *MenuController) ReplaceRootItems(items []MenuItem) {
	c.mu.Lock()
	c.root.ReplaceItems(items)
	active := c.menu == c.root
	if active {
		c.resetScrollLocked(time.Now())
		c.resetSaverLocked(time.Now())
	}
	c.mu.Unlock()
	if active {
		c.RequestRedraw()
	}
}

func (c *MenuController) Run(ctx context.Context) error {
	scroller := time.NewTicker(scrollInterval)
	defer scroller.Stop()
	transitionTicker := time.NewTicker(transitionInterval)
	defer transitionTicker.Stop()
	var messageTimer *time.Timer
	var messageExpired <-chan time.Time
	defer func() {
		if messageTimer != nil {
			messageTimer.Stop()
		}
	}()
	for {
		select {
		case <-ctx.Done():
			return nil
		case <-c.redraw:
			menu, message, until, scroll, saveRow, transition := c.renderState()
			frame := c.renderer.Render(menu, scroll)
			if transition != nil {
				from := c.renderer.Render(transition.from, 0)
				to := c.renderer.Render(transition.to, 0)
				left := transition.left
				if c.renderer.rotation == 180 {
					left = !left
				}
				frame = oled.PushFrame(from, to, DisplayWidth*transition.step/transitionFrameStep, left)
			}
			if saveRow >= 0 {
				frame = c.renderer.RenderScreenSaver(screenSaverLine(), saveRow)
			} else if message != "" && time.Now().Before(until) {
				frame = c.renderer.RenderMessage(message)
				wait := time.Until(until)
				if messageTimer == nil {
					messageTimer = time.NewTimer(wait)
				} else {
					if !messageTimer.Stop() {
						select {
						case <-messageTimer.C:
						default:
						}
					}
					messageTimer.Reset(wait)
				}
				messageExpired = messageTimer.C
			}
			if err := c.present.Present(frame); err != nil {
				return fmt.Errorf("present menu: %w", err)
			}
		case <-messageExpired:
			c.mu.Lock()
			if !c.until.After(time.Now()) {
				c.message = ""
				c.until = time.Time{}
			}
			c.mu.Unlock()
			messageExpired = nil
			c.RequestRedraw()
		case now := <-scroller.C:
			if c.advanceScreenSaver(now) || c.advanceScroll(now) {
				c.RequestRedraw()
			}
		case <-transitionTicker.C:
			if c.advanceTransition() {
				c.requestRedraw()
			}
		}
	}
}

func (c *MenuController) renderState() (*Menu, string, time.Time, int, int, *menuTransition) {
	c.mu.Lock()
	defer c.mu.Unlock()
	var transition *menuTransition
	if c.transition != nil {
		transition = &menuTransition{
			from: cloneMenu(c.transition.from),
			to:   cloneMenu(c.transition.to),
			left: c.transition.left,
			step: c.transition.step,
		}
	}
	return cloneMenu(c.menu), c.message, c.until, c.scroll, c.saveRow, transition
}

func (c *MenuController) startTransitionLocked(from, to *Menu, left bool) {
	if !c.transitions {
		c.transition = nil
		return
	}
	c.transition = &menuTransition{from: from, to: cloneMenu(to), left: left, step: 1}
}

func (c *MenuController) advanceTransition() bool {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.transition == nil || c.transition.step >= transitionFrameStep {
		c.transition = nil
		return false
	}
	c.transition.step++
	return true
}

func cloneMenu(menu *Menu) *Menu {
	if menu == nil {
		return nil
	}
	copy := *menu
	copy.Items = append([]MenuItem(nil), menu.Items...)
	return &copy
}

func (c *MenuController) resetScrollLocked(now time.Time) {
	c.scroll, c.lastMove = 0, now
}

func (c *MenuController) resetSaverLocked(now time.Time) {
	c.saveRow = -1
	if c.saver != nil {
		c.saver.Reset(now)
	}
}

func (c *MenuController) advanceScreenSaver(now time.Time) bool {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.saver == nil || (c.message != "" && now.Before(c.until)) {
		return false
	}
	active, redraw := c.saver.Update(now, c.renderer.Rows())
	if active {
		c.saveRow = c.saver.row
	}
	return redraw
}

func (c *MenuController) advanceScroll(now time.Time) bool {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.message != "" && now.Before(c.until) || now.Sub(c.lastMove) < scrollStartDelay {
		return false
	}
	selected, ok := c.menu.Selected()
	if !ok {
		return false
	}
	period := c.renderer.ScrollPeriod(selected.Label)
	if period == 0 {
		return false
	}
	c.scroll = (c.scroll + 1) % period
	return true
}
