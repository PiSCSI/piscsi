package assets

import "testing"

func TestTypeWriterFontReturnsIndependentBytes(t *testing.T) {
	assertIndependentFontBytes(t, TypeWriterFont, "TypeWriterFont")
}

func TestDejaVuSansMonoBoldFontReturnsIndependentBytes(t *testing.T) {
	assertIndependentFontBytes(t, DejaVuSansMonoBoldFont, "DejaVuSansMonoBoldFont")
}

func assertIndependentFontBytes(t *testing.T, load func() []byte, name string) {
	t.Helper()
	first, second := load(), load()
	if len(first) == 0 || len(second) == 0 {
		t.Fatalf("%s returned an empty embedded font", name)
	}
	first[0] ^= 0xff
	if first[0] == second[0] {
		t.Fatalf("%s returned shared bytes", name)
	}
}

func TestSplashSupportsBothHeightsAndStates(t *testing.T) {
	for _, start := range []bool{false, true} {
		for _, height := range []int{32, 64} {
			image, err := Splash(start, height)
			if err != nil {
				t.Fatalf("Splash(%t, %d): %v", start, height, err)
			}
			if image.Bounds().Dx() != 128 || image.Bounds().Dy() != height {
				t.Fatalf("Splash(%t, %d) bounds = %v", start, height, image.Bounds())
			}
		}
	}
	if _, err := Splash(true, 48); err == nil {
		t.Fatal("Splash accepted unsupported height")
	}
}
