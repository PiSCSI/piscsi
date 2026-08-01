package ctrlboard

import (
	"context"
	"testing"
	"time"

	oled "github.com/piscsi/piscsi/go/piscsi-oled"
)

type fakePresenter struct{ frames chan oled.Frame }

func (p *fakePresenter) Present(frame oled.Frame) error { p.frames <- frame; return nil }

func TestMenuControllerNavigationSchedulesRendering(t *testing.T) {
	renderer, err := NewRenderer(0)
	if err != nil {
		t.Fatal(err)
	}
	defer renderer.Close()
	menu := testMenu(t, 2, renderer.Rows())
	presenter := &fakePresenter{frames: make(chan oled.Frame, 2)}
	controller, err := NewMenuController(menu, renderer, presenter)
	if err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan error, 1)
	go func() { done <- controller.Run(ctx) }()
	if !controller.Handle(Event{Type: EventRotateClockwise}) {
		t.Fatal("rotation did not change selection")
	}
	select {
	case <-presenter.frames:
	case <-time.After(time.Second):
		t.Fatal("navigation did not schedule a frame")
	}
	if index := controller.Snapshot().SelectedIndex(); index != 1 {
		t.Fatalf("selected index = %d, want 1", index)
	}
	cancel()
	if err := <-done; err != nil {
		t.Fatal(err)
	}
}

func TestMenuControllerSubmenuPreservesRootRefresh(t *testing.T) {
	renderer, err := NewRenderer(0)
	if err != nil {
		t.Fatal(err)
	}
	defer renderer.Close()
	root := testMenu(t, 2, renderer.Rows())
	presenter := &fakePresenter{frames: make(chan oled.Frame, 4)}
	controller, err := NewMenuController(root, renderer, presenter)
	if err != nil {
		t.Fatal(err)
	}
	submenu, err := NewMenu("child", []MenuItem{{ID: "child", Label: "child"}}, renderer.Rows())
	if err != nil {
		t.Fatal(err)
	}
	if err := controller.Push(submenu); err != nil {
		t.Fatal(err)
	}
	controller.ReplaceRootItems([]MenuItem{{ID: "replacement", Label: "replacement"}})
	if got := controller.Snapshot().Title; got != "child" {
		t.Fatalf("active menu = %q, want submenu", got)
	}
	if !controller.Pop() {
		t.Fatal("Pop did not return to root")
	}
	if got := controller.Snapshot().Items[0].ID; got != "replacement" {
		t.Fatalf("root item = %q", got)
	}
}

func TestMenuControllerTransitionsPushAndPopAndCancelsOnRedraw(t *testing.T) {
	renderer, err := NewRenderer(0)
	if err != nil {
		t.Fatal(err)
	}
	defer renderer.Close()
	root := testMenu(t, 2, renderer.Rows())
	controller, err := NewMenuController(root, renderer, &fakePresenter{frames: make(chan oled.Frame, 1)})
	if err != nil {
		t.Fatal(err)
	}
	controller.SetTransitions(true)
	child, err := NewMenu("child", []MenuItem{{ID: "child", Label: "child"}}, renderer.Rows())
	if err != nil {
		t.Fatal(err)
	}
	if err := controller.Push(child); err != nil {
		t.Fatal(err)
	}
	_, _, _, _, _, transition := controller.renderState()
	if transition == nil || !transition.left || transition.step != 1 {
		t.Fatalf("push transition = %#v", transition)
	}
	controller.ClearMessage()
	_, _, _, _, _, transition = controller.renderState()
	if transition == nil {
		t.Fatal("clearing a status message cancelled the transition")
	}
	controller.RequestRedraw()
	_, _, _, _, _, transition = controller.renderState()
	if transition != nil {
		t.Fatalf("transition was not cancelled by redraw: %#v", transition)
	}
	if !controller.Pop() {
		t.Fatal("Pop did not return to root")
	}
	_, _, _, _, _, transition = controller.renderState()
	if transition == nil || transition.left {
		t.Fatalf("pop transition = %#v", transition)
	}
}

func TestMenuControllerScrollsOnlyLongIdleSelection(t *testing.T) {
	renderer, err := NewRenderer(0)
	if err != nil {
		t.Fatal(err)
	}
	defer renderer.Close()
	menu, err := NewMenu("root", []MenuItem{{ID: "long", Label: "a deliberately long menu entry that exceeds the display width"}}, renderer.Rows())
	if err != nil {
		t.Fatal(err)
	}
	controller, err := NewMenuController(menu, renderer, &fakePresenter{frames: make(chan oled.Frame, 1)})
	if err != nil {
		t.Fatal(err)
	}
	now := time.Now()
	if controller.advanceScroll(now) {
		t.Fatal("scroll began before the idle delay")
	}
	if !controller.advanceScroll(now.Add(scrollStartDelay)) {
		t.Fatal("long idle selection did not begin scrolling")
	}
	if _, _, _, offset, _, _ := controller.renderState(); offset != 1 {
		t.Fatalf("scroll offset = %d, want 1", offset)
	}
}
