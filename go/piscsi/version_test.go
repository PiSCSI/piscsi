package piscsi

import "testing"

func TestVersionBanner(t *testing.T) {
	want := "piscsi-test\nVersion " + ProjectVersion + "\n" + Copyright + "\n"
	if got := VersionBanner("piscsi-test"); got != want {
		t.Errorf("VersionBanner() = %q, want %q", got, want)
	}
}
