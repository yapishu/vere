// bridge is the galaxy-side WebTransport↔UDP ames bridge (phase 0 of
// doc/spec/ames-over-quic.md in the urbit repo).
//
// Each accepted WebTransport session gets its own UDP socket to the ship's
// ames port, so the ship sees each remote peer as a distinct local lane and
// the UDP 4-tuple provides the return path: the bridge is fully transparent
// and keeps no ship-level state. Because ames packets are end-to-end
// authenticated, the bridge is trustless for content.
//
// Certificates: pass -cert/-key for a real (webpki) cert, or -dev to mint a
// self-signed ECDSA cert valid ≤14 days, usable from browsers via
// serverCertificateHashes; its DER sha-256 is printed on startup.
package main

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/sha256"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/base64"
	"flag"
	"log"
	"math/big"
	"net"
	"net/http"
	"time"

	"github.com/quic-go/quic-go/http3"
	"github.com/quic-go/webtransport-go"

	"ames-wt-bridge/internal/pump"
)

func main() {
	var (
		listen = flag.String("listen", ":8443", "UDP address to serve WebTransport on")
		ames   = flag.String("ames", "127.0.0.1:31337", "ship's ames UDP address")
		path   = flag.String("path", "/~_~/ames", "WebTransport CONNECT path")
		cert   = flag.String("cert", "", "TLS certificate file (webpki)")
		key    = flag.String("key", "", "TLS key file (webpki)")
		dev    = flag.Bool("dev", false, "use a self-signed short-lived certificate")
	)
	flag.Parse()

	amesAddr, err := net.ResolveUDPAddr("udp", *ames)
	if err != nil {
		log.Fatalf("bridge: bad -ames address: %v", err)
	}

	tlsConf, err := tlsConfig(*cert, *key, *dev)
	if err != nil {
		log.Fatalf("bridge: tls: %v", err)
	}

	srv := &webtransport.Server{
		H3: &http3.Server{
			Addr:            *listen,
			TLSConfig:       http3.ConfigureTLSConfig(tlsConf),
			EnableDatagrams: true,
		},
		// any web origin may dial: the bridge carries only end-to-end
		// authenticated ames packets, so origin-based CSRF protection
		// adds nothing here
		CheckOrigin: func(*http.Request) bool { return true },
	}
	webtransport.ConfigureHTTP3Server(srv.H3)

	http.HandleFunc(*path, func(w http.ResponseWriter, r *http.Request) {
		sess, err := srv.Upgrade(w, r)
		if err != nil {
			log.Printf("bridge: upgrade: %v", err)
			w.WriteHeader(http.StatusInternalServerError)
			return
		}
		go serve(sess, amesAddr)
	})

	log.Printf("bridge: webtransport on %s%s -> ames at %s", *listen, *path, *ames)
	log.Fatal(srv.ListenAndServe())
}

// serve pumps one session against its own socket to the ship.
func serve(sess *webtransport.Session, ames *net.UDPAddr) {
	conn, err := net.DialUDP("udp", nil, ames)
	if err != nil {
		log.Printf("bridge: udp dial: %v", err)
		sess.CloseWithError(1, "ames unreachable")
		return
	}
	log.Printf("bridge: session %s -> %s", sess.RemoteAddr(), conn.LocalAddr())

	ctx := sess.Context()
	go pump.UDPToSession(ctx, conn, sess)
	pump.SessionToUDP(ctx, sess, conn)

	conn.Close()
	log.Printf("bridge: session %s closed", sess.RemoteAddr())
}

func tlsConfig(cert, key string, dev bool) (*tls.Config, error) {
	if !dev {
		crt, err := tls.LoadX509KeyPair(cert, key)
		if err != nil {
			return nil, err
		}
		return &tls.Config{Certificates: []tls.Certificate{crt}}, nil
	}

	// self-signed, ECDSA, ≤14 days: the WebTransport
	// serverCertificateHashes profile.
	priv, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return nil, err
	}
	// Chromium's serverCertificateHashes requires total validity ≤ 14 days;
	// stay comfortably under, counting the backdate.
	tmpl := x509.Certificate{
		SerialNumber: big.NewInt(time.Now().UnixNano()),
		Subject:      pkix.Name{CommonName: "ames-wt-bridge"},
		NotBefore:    time.Now().Add(-time.Hour),
		NotAfter:     time.Now().Add(13 * 24 * time.Hour),
		DNSNames:     []string{"localhost"},
		IPAddresses:  []net.IP{net.IPv4(127, 0, 0, 1)},
	}
	der, err := x509.CreateCertificate(rand.Reader, &tmpl, &tmpl, &priv.PublicKey, priv)
	if err != nil {
		return nil, err
	}
	leaf, err := x509.ParseCertificate(der)
	if err != nil {
		return nil, err
	}
	// the WebTransport serverCertificateHashes API pins the sha-256 of
	// the whole DER certificate (not the SPKI)
	sum := sha256.Sum256(der)
	log.Printf("bridge: dev cert sha-256: %s",
		base64.StdEncoding.EncodeToString(sum[:]))

	return &tls.Config{
		Certificates: []tls.Certificate{{
			Certificate: [][]byte{der},
			PrivateKey:  priv,
			Leaf:        leaf,
		}},
	}, nil
}
