// bridge gives each WebTransport session its own ordinary UDP socket.
//
// Browser->gateway frames contain an Ames destination lane and packet.
// Gateway->browser frames contain the actual UDP source lane and packet. The
// rest of the network therefore sees an entirely ordinary UDP Ames endpoint;
// no peer, sponsor, galaxy, kernel, or native Vere changes are required.
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
	"errors"
	"flag"
	"fmt"
	"log"
	"math/big"
	"net"
	"net/http"
	"strings"
	"sync"
	"time"

	"github.com/quic-go/quic-go/http3"
	"github.com/quic-go/webtransport-go"

	"ames-wt-bridge/internal/gateway"
	"ames-wt-bridge/internal/pump"
)

const galaxySuffixes = "" +
	"zodnecbudwessevpersutletfulpensytdurwepserwylsun" +
	"rypsyxdyrnuphebpeglupdepdysputlughecryttyvsydnex" +
	"lunmeplutseppesdelsulpedtemledtulmetwenbynhexfeb" +
	"pyldulhetmevruttylwydtepbesdexsefwycburderneppur" +
	"rysrebdennutsubpetrulsynregtydsupsemwynrecmegnet" +
	"secmulnymtevwebsummutnyxrextebfushepbenmuswyxsym" +
	"selrucdecwexsyrwetdylmynmesdetbetbeltuxtugmyrpel" +
	"syptermebsetdutdegtexsurfeltudnuxruxrenwytnubmed" +
	"lytdusnebrumtynseglyxpunresredfunrevrefmectedrus" +
	"bexlebduxrynnumpyxrygryxfeptyrtustyclegnemfermer" +
	"tenlusnussyltecmexpubrymtucfyllepdebbermughuttun" +
	"bylsudpemdevlurdefbusbeprunmelpexdytbyttyplevmyl" +
	"wedducfurfexnulluclennerlexrupnedlecrydlydfenwel" +
	"nydhusrelrudneshesfetdesretdunlernyrsebhulryllud" +
	"remlysfynwerrycsugnysnyllyndyndemluxfedsedbecmun" +
	"lyrtesmudnytbyrsenwegfyrmurtelreptegpecnelnevfes"

func main() {
	var (
		listen         = flag.String("listen", ":8443", "UDP address to serve WebTransport on")
		path           = flag.String("path", "/~_~/ames", "WebTransport CONNECT path")
		token          = flag.String("token", "", "optional shared token required as the URL token query parameter")
		cert           = flag.String("cert", "", "TLS certificate file (webpki)")
		key            = flag.String("key", "", "TLS key file (webpki)")
		dev            = flag.Bool("dev", false, "use a self-signed short-lived certificate")
		udpIP          = flag.String("udp-ip", "0.0.0.0", "local IPv4 address for per-session UDP sockets")
		udpMin         = flag.Int("udp-port-min", 0, "first UDP port to allocate (0 uses ephemeral ports)")
		udpMax         = flag.Int("udp-port-max", 0, "last UDP port to allocate (0 uses ephemeral ports)")
		domain         = flag.String("ames-domain", "urbit.org", "galaxy Ames DNS domain")
		galaxyBasePort = flag.Int("galaxy-base-port", 13337, "UDP port for ~zod; galaxy number is added")
		logPackets     = flag.Bool("log-packets", false, "log compact per-packet WebTransport/UDP routing")
	)
	flag.Parse()
	if (*udpMin == 0) != (*udpMax == 0) || *udpMin < 0 || *udpMax < *udpMin || *udpMax > 65535 {
		log.Fatal("bridge: UDP port range must be 0/0 or a valid inclusive range")
	}
	ip := net.ParseIP(*udpIP).To4()
	if ip == nil {
		log.Fatalf("bridge: -udp-ip must be an IPv4 address: %q", *udpIP)
	}
	if *galaxyBasePort <= 0 || *galaxyBasePort+255 > 65535 {
		log.Fatal("bridge: invalid -galaxy-base-port")
	}
	allocator := &udpAllocator{ip: ip, min: *udpMin, max: *udpMax, next: *udpMin}
	resolver := &galaxyResolver{
		domain:   strings.Trim(*domain, "."),
		basePort: *galaxyBasePort,
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
		if *token != "" && r.URL.Query().Get("token") != *token {
			http.Error(w, "unauthorized", http.StatusUnauthorized)
			return
		}
		sess, err := srv.Upgrade(w, r)
		if err != nil {
			log.Printf("bridge: upgrade: %v", err)
			w.WriteHeader(http.StatusInternalServerError)
			return
		}
		go serve(sess, allocator, resolver, *logPackets)
	})
	if *token == "" {
		log.Printf("bridge: WARNING no -token set; this endpoint is an unauthenticated UDP relay")
	}

	log.Printf(
		"bridge: WebTransport %s%s; one UDP socket per session on %s ports %d-%d",
		*listen,
		*path,
		ip,
		*udpMin,
		*udpMax,
	)
	log.Fatal(srv.ListenAndServe())
}

type udpAllocator struct {
	mu       sync.Mutex
	ip       net.IP
	min, max int
	next     int
}

func (a *udpAllocator) listen() (*net.UDPConn, error) {
	a.mu.Lock()
	defer a.mu.Unlock()
	if a.min == 0 {
		return net.ListenUDP("udp4", &net.UDPAddr{IP: a.ip})
	}
	count := a.max - a.min + 1
	for range count {
		port := a.next
		a.next++
		if a.next > a.max {
			a.next = a.min
		}
		conn, err := net.ListenUDP("udp4", &net.UDPAddr{IP: a.ip, Port: port})
		if err == nil {
			return conn, nil
		}
	}
	return nil, fmt.Errorf("no free UDP port in %d-%d", a.min, a.max)
}

type galaxyResolver struct {
	mu       sync.Mutex
	domain   string
	basePort int
	cache    [256]*net.UDPAddr
}

func (r *galaxyResolver) resolve(ship byte) (*net.UDPAddr, error) {
	r.mu.Lock()
	defer r.mu.Unlock()
	if cached := r.cache[ship]; cached != nil {
		return cached, nil
	}
	offset := int(ship) * 3
	host := galaxySuffixes[offset:offset+3] + "." + r.domain
	addr, err := net.ResolveUDPAddr("udp4", fmt.Sprintf("%s:%d", host, r.basePort+int(ship)))
	if err != nil {
		return nil, err
	}
	r.cache[ship] = addr
	return addr, nil
}

func resolveLane(lane gateway.Lane, galaxies *galaxyResolver) (*net.UDPAddr, error) {
	if lane.Galaxy != nil {
		return galaxies.resolve(*lane.Galaxy)
	}
	if lane.IPv4.To4() == nil || lane.Port == 0 {
		return nil, errors.New("invalid IPv4 destination lane")
	}
	return &net.UDPAddr{IP: lane.IPv4.To4(), Port: int(lane.Port)}, nil
}

// serve pumps one browser ship against its own public-facing UDP socket.
func serve(
	sess *webtransport.Session,
	allocator *udpAllocator,
	galaxies *galaxyResolver,
	logPackets bool,
) {
	conn, err := allocator.listen()
	if err != nil {
		log.Printf("bridge: UDP allocate: %v", err)
		sess.CloseWithError(1, "no UDP socket available")
		return
	}
	log.Printf("bridge: session %s owns UDP %s", sess.RemoteAddr(), conn.LocalAddr())

	ctx := sess.Context()
	go func() {
		buf := make([]byte, 65536)
		for {
			n, source, err := conn.ReadFromUDP(buf)
			if err != nil {
				return
			}
			if logPackets {
				log.Printf("bridge: UDP %s -> session %s %dB", source, sess.RemoteAddr(), n)
			}
			frame, err := gateway.Encode(
				gateway.FrameHear,
				gateway.IPv4Lane(source.IP, uint16(source.Port)),
				buf[:n],
			)
			if err == nil {
				pump.SendPacket(ctx, sess, frame)
			}
		}
	}()
	pump.ReceiveToFunc(ctx, sess, func(input []byte) {
		frame, err := gateway.Decode(input)
		if err != nil || frame.Type != gateway.FrameSend {
			if err != nil {
				log.Printf("bridge: drop malformed frame from %s: %v", sess.RemoteAddr(), err)
			}
			return
		}
		destination, err := resolveLane(frame.Lane, galaxies)
		if err != nil {
			log.Printf("bridge: drop unresolved lane from %s: %v", sess.RemoteAddr(), err)
			return
		}
		if _, err := conn.WriteToUDP(frame.Packet, destination); err != nil {
			log.Printf("bridge: UDP send %s: %v", destination, err)
		} else if logPackets {
			log.Printf("bridge: session %s -> UDP %s %dB", sess.RemoteAddr(), destination, len(frame.Packet))
		}
	})

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
