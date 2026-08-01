package ctrlboard

import "testing"

func TestSlotActionMenuRetainsSlotContext(t *testing.T) {
	menu, err := NewSlotActionMenu(SCSISlot{ID: 4, Reserved: true}, 4)
	if err != nil {
		t.Fatal(err)
	}
	if len(menu.Items) != 8 || menu.Items[0].Label != "Return" {
		t.Fatalf("menu items = %#v", menu.Items)
	}
	action, ok := menu.Items[2].Data.(SlotAction)
	if !ok || action.Kind != SlotActionDetachEject || action.Slot.ID != 4 {
		t.Fatalf("action = %#v", menu.Items[2].Data)
	}
	reservation, ok := menu.Items[4].Data.(SlotAction)
	if !ok || reservation.Kind != SlotActionRelease || reservation.Slot.ID != 4 {
		t.Fatalf("reservation action = %#v", menu.Items[4].Data)
	}
}

func TestSlotActionMenuOffersReserveForAvailableSlot(t *testing.T) {
	menu, err := NewSlotActionMenu(SCSISlot{ID: 4}, 4)
	if err != nil {
		t.Fatal(err)
	}
	action, ok := menu.Items[4].Data.(SlotAction)
	if !ok || action.Kind != SlotActionReserve || action.Slot.ID != 4 {
		t.Fatalf("reservation action = %#v", menu.Items[4].Data)
	}
}
