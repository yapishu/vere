package main

import (
	"net"
	"testing"
)

func TestUDPAllocatorGivesConcurrentSessionsDistinctPorts(t *testing.T) {
	allocator := &udpAllocator{ip: net.ParseIP("127.0.0.1").To4()}
	first, err := allocator.listen()
	if err != nil {
		t.Fatal(err)
	}
	defer first.Close()
	second, err := allocator.listen()
	if err != nil {
		t.Fatal(err)
	}
	defer second.Close()

	firstPort := first.LocalAddr().(*net.UDPAddr).Port
	secondPort := second.LocalAddr().(*net.UDPAddr).Port
	if firstPort == secondPort {
		t.Fatalf("concurrent sessions shared UDP port %d", firstPort)
	}
}
