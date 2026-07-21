// dial is the client-side counterpart of the ames WebTransport bridge: it
// binds a local UDP port and tunnels ames packets over a WebTransport
// session to a bridge. It stands in for a browser peer until the wasm
// runtime exists, and lets a native ship reach a bridge-fronted ship
// through the QUIC hop.
//
// Return traffic goes to whichever local UDP source spoke most recently —
// the dialer serves one local ship.
package main

import (
	"context"
	"crypto/tls"
	"flag"
	"log"
	"net"
	"sync/atomic"

	"github.com/quic-go/webtransport-go"

	"ames-wt-bridge/internal/pump"
)

func main() {
	var (
		udp      = flag.String("udp", "127.0.0.1:31337", "local UDP address to listen on")
		url      = flag.String("url", "https://127.0.0.1:8443/~_~/ames", "bridge WebTransport URL")
		insecure = flag.Bool("insecure", false, "skip TLS verification (dev bridges)")
		verbose  = flag.Bool("verbose", false, "log packet hex")
	)
	flag.Parse()
	pump.Verbose = *verbose

	local, err := net.ResolveUDPAddr("udp", *udp)
	if err != nil {
		log.Fatalf("dial: bad -udp address: %v", err)
	}
	conn, err := net.ListenUDP("udp", local)
	if err != nil {
		log.Fatalf("dial: udp listen: %v", err)
	}

	d := webtransport.Dialer{
		TLSClientConfig: &tls.Config{InsecureSkipVerify: *insecure},
	}
	ctx := context.Background()
	_, sess, err := d.Dial(ctx, *url, nil)
	if err != nil {
		log.Fatalf("dial: webtransport dial: %v", err)
	}
	log.Printf("dial: udp %s <-> %s", *udp, *url)

	// last local speaker gets the return traffic
	var peer atomic.Pointer[net.UDPAddr]

	go func() {
		buf := make([]byte, 65536)
		for {
			n, from, err := conn.ReadFromUDP(buf)
			if err != nil {
				return
			}
			peer.Store(from)
			pump.SendPacket(sess.Context(), sess, buf[:n])
		}
	}()

	pump.ReceiveToFunc(sess.Context(), sess, func(pkt []byte) {
		to := peer.Load()
		if to == nil {
			return // nobody local has spoken yet
		}
		if _, err := conn.WriteToUDP(pkt, to); err != nil {
			log.Printf("dial: udp write: %v", err)
		}
	})
	log.Printf("dial: session closed")
}
