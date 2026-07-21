// rawdial binds a local UDP port and tunnels ames packets over a raw QUIC
// ames/1 session to a native Vere endpoint.
//
// It is the raw-QUIC counterpart of cmd/dial. The first local UDP sender gets
// the return traffic, so this tool serves one local ship.
package main

import (
	"context"
	"crypto/tls"
	"flag"
	"log"
	"net"
	"sync/atomic"
	"time"

	"github.com/quic-go/quic-go"

	"ames-wt-bridge/internal/pump"
)

func main() {
	var (
		udp        = flag.String("udp", "127.0.0.1:31337", "local UDP address to listen on")
		addr       = flag.String("addr", "127.0.0.1:8443", "raw QUIC ames/1 address")
		verify     = flag.Bool("verify", false, "verify the server TLS certificate")
		serverName = flag.String("server-name", "localhost", "TLS server name")
		verbose    = flag.Bool("verbose", false, "log packet hex")
	)
	flag.Parse()
	pump.Verbose = *verbose

	local, err := net.ResolveUDPAddr("udp", *udp)
	if err != nil {
		log.Fatalf("rawdial: bad -udp address: %v", err)
	}
	conn, err := net.ListenUDP("udp", local)
	if err != nil {
		log.Fatalf("rawdial: udp listen: %v", err)
	}
	defer conn.Close()

	ctx := context.Background()
	qconn, err := quic.DialAddr(ctx, *addr, &tls.Config{
		NextProtos:         []string{"ames/1"},
		ServerName:         *serverName,
		InsecureSkipVerify: !*verify,
	}, &quic.Config{
		Versions:              []quic.Version{quic.Version1},
		EnableDatagrams:       true,
		KeepAlivePeriod:       15 * time.Second,
		MaxIncomingUniStreams: 16,
	})
	if err != nil {
		log.Fatalf("rawdial: quic dial: %v", err)
	}
	defer qconn.CloseWithError(0, "")

	state := qconn.ConnectionState()
	if !state.SupportsDatagrams.Remote {
		log.Printf("rawdial: peer did not advertise DATAGRAM support; using streams")
	}
	log.Printf("rawdial: udp %s <-> raw QUIC %s", *udp, *addr)

	var peer atomic.Pointer[net.UDPAddr]

	go func() {
		buf := make([]byte, 65536)
		for {
			n, from, err := conn.ReadFromUDP(buf)
			if err != nil {
				return
			}
			peer.Store(from)
			pump.SendQUICPacket(qconn.Context(), qconn, buf[:n])
		}
	}()

	pump.QUICReceiveToFunc(qconn.Context(), qconn, func(pkt []byte) {
		to := peer.Load()
		if to == nil {
			return // nobody local has spoken yet
		}
		if _, err := conn.WriteToUDP(pkt, to); err != nil {
			log.Printf("rawdial: udp write: %v", err)
		}
	})
	log.Printf("rawdial: session closed")
}
