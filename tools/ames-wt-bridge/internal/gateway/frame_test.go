package gateway

import (
	"bytes"
	"net"
	"testing"
)

func TestGalaxyFrameRoundTrip(t *testing.T) {
	input := []byte{1, 2, 3}
	encoded, err := Encode(FrameSend, GalaxyLane(0), input)
	if err != nil {
		t.Fatal(err)
	}
	frame, err := Decode(encoded)
	if err != nil {
		t.Fatal(err)
	}
	if frame.Type != FrameSend || frame.Lane.Galaxy == nil || *frame.Lane.Galaxy != 0 {
		t.Fatalf("unexpected frame: %#v", frame)
	}
	if !bytes.Equal(frame.Packet, input) {
		t.Fatalf("packet = %v, want %v", frame.Packet, input)
	}
}

func TestIPv4FrameRoundTrip(t *testing.T) {
	lane := IPv4Lane(net.IPv4(127, 0, 0, 1), 13337)
	encoded, err := Encode(FrameHear, lane, []byte{4, 5})
	if err != nil {
		t.Fatal(err)
	}
	frame, err := Decode(encoded)
	if err != nil {
		t.Fatal(err)
	}
	if frame.Type != FrameHear ||
		!frame.Lane.IPv4.Equal(net.IPv4(127, 0, 0, 1)) ||
		frame.Lane.Port != 13337 {
		t.Fatalf("unexpected frame: %#v", frame)
	}
}

func TestDecodeRejectsMalformedFrame(t *testing.T) {
	if _, err := Decode([]byte{1, 2, 3}); err == nil {
		t.Fatal("expected truncated frame error")
	}
	encoded, err := Encode(FrameSend, GalaxyLane(0), []byte{1})
	if err != nil {
		t.Fatal(err)
	}
	encoded[0] = 0
	if _, err := Decode(encoded); err == nil {
		t.Fatal("expected magic error")
	}
}
