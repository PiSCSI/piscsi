package main

import "testing"

func TestReplaceFormatDirectives(t *testing.T) {
	got := replaceFormatDirectives(
		"Detached device from SCSI ID %s:%d (100%% complete)",
		nil,
	)
	want := "Detached device from SCSI ID %(value_1)s:%(value_2)s (100% complete)"
	if got != want {
		t.Fatalf("replaceFormatDirectives() = %q, want %q", got, want)
	}
}

func TestCleanTemplateText(t *testing.T) {
	got := cleanTemplateText("  Current {{.ProductName}}  Configuration\n")
	if got != "Current Configuration" {
		t.Fatalf("cleanTemplateText() = %q, want %q", got, "Current Configuration")
	}
}
