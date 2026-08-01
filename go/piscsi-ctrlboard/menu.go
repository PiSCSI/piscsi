// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import "fmt"

// MenuItem is a selectable row. ID is stable across refreshes; Data belongs
// to the menu builder and is deliberately opaque to navigation and rendering.
type MenuItem struct {
	ID    string
	Label string
	Data  any
}

// Menu keeps selection and page state independent from the renderer. It does
// not wrap at either end, preserving the established Control Board behavior.
type Menu struct {
	Title    string
	Items    []MenuItem
	selected int
	firstRow int
	pageSize int
}

func NewMenu(title string, items []MenuItem, pageSize int) (*Menu, error) {
	if pageSize <= 0 {
		return nil, fmt.Errorf("menu page size must be positive")
	}
	cloned := append([]MenuItem(nil), items...)
	return &Menu{Title: title, Items: cloned, pageSize: pageSize}, nil
}

func (m *Menu) SelectedIndex() int { return m.selected }
func (m *Menu) FirstVisible() int  { return m.firstRow }

func (m *Menu) Selected() (MenuItem, bool) {
	if m == nil || len(m.Items) == 0 {
		return MenuItem{}, false
	}
	return m.Items[m.selected], true
}

// Move changes selection by delta, clamping at the ends and updating the
// current page. It reports whether selection changed.
func (m *Menu) Move(delta int) bool {
	if m == nil || len(m.Items) == 0 || delta == 0 {
		return false
	}
	next := m.selected + delta
	if next < 0 {
		next = 0
	}
	if next >= len(m.Items) {
		next = len(m.Items) - 1
	}
	if next == m.selected {
		return false
	}
	m.selected = next
	m.ensureVisible()
	return true
}

// ReplaceItems refreshes menu contents while retaining the selected item's ID
// where possible, which avoids jumpy selection during asynchronous refreshes.
func (m *Menu) ReplaceItems(items []MenuItem) {
	if m == nil {
		return
	}
	selectedID := ""
	if selected, ok := m.Selected(); ok {
		selectedID = selected.ID
	}
	m.Items = append([]MenuItem(nil), items...)
	m.selected, m.firstRow = 0, 0
	for index, item := range m.Items {
		if selectedID != "" && item.ID == selectedID {
			m.selected = index
			break
		}
	}
	m.ensureVisible()
}

// Visible returns the current page in menu order.
func (m *Menu) Visible() []MenuItem {
	if m == nil || len(m.Items) == 0 {
		return nil
	}
	end := min(m.firstRow+m.pageSize, len(m.Items))
	return m.Items[m.firstRow:end]
}

func (m *Menu) ensureVisible() {
	if len(m.Items) == 0 {
		m.selected, m.firstRow = 0, 0
		return
	}
	if m.selected >= m.firstRow+m.pageSize {
		m.firstRow += m.pageSize/2 + 1
	}
	if m.selected < m.firstRow {
		m.firstRow -= m.pageSize/2 + 1
	}
	maxFirst := max(0, len(m.Items)-m.pageSize)
	m.firstRow = min(max(m.firstRow, 0), maxFirst)
}
