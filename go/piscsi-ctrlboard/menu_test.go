package ctrlboard

import (
	"fmt"
	"testing"
)

func testMenu(t *testing.T, count, pageSize int) *Menu {
	t.Helper()
	items := make([]MenuItem, count)
	for index := range items {
		items[index] = MenuItem{ID: fmt.Sprint(index), Label: fmt.Sprintf("item %d", index)}
	}
	menu, err := NewMenu("test", items, pageSize)
	if err != nil {
		t.Fatal(err)
	}
	return menu
}

func TestMenuClampsSelectionAndPages(t *testing.T) {
	menu := testMenu(t, 8, 4)
	if menu.Move(-1) || menu.SelectedIndex() != 0 {
		t.Fatal("menu moved before its first item")
	}
	for range 4 {
		menu.Move(1)
	}
	if menu.SelectedIndex() != 4 || menu.FirstVisible() != 3 {
		t.Fatalf("selection/page = %d/%d, want 4/3", menu.SelectedIndex(), menu.FirstVisible())
	}
	for range 10 {
		menu.Move(1)
	}
	if menu.SelectedIndex() != 7 || menu.FirstVisible() != 4 {
		t.Fatalf("selection/page = %d/%d, want 7/4", menu.SelectedIndex(), menu.FirstVisible())
	}
}

func TestMenuRefreshRetainsSelectedID(t *testing.T) {
	menu := testMenu(t, 4, 4)
	menu.Move(2)
	menu.ReplaceItems([]MenuItem{{ID: "new", Label: "new"}, {ID: "2", Label: "retained"}})
	selected, ok := menu.Selected()
	if !ok || selected.ID != "2" || menu.SelectedIndex() != 1 {
		t.Fatalf("selected = %#v at %d", selected, menu.SelectedIndex())
	}
}
