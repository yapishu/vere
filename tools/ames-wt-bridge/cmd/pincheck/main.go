// pincheck dials the bridge verifying the server only by its certificate-DER
// SHA-256 — the same trust model as the browser's serverCertificateHashes. It
// isolates whether a dev-cert WebTransport dial fails because of the SPKI hash
// (our bug) or the browser's policy (environment). If this connects with the
// bridge's printed hash, the hash is correct.
package main

import (
	"context"
	"crypto/sha256"
	"crypto/tls"
	"encoding/base64"
	"errors"
	"flag"
	"log"

	"github.com/quic-go/webtransport-go"
)

func main() {
	url := flag.String("url", "https://127.0.0.1:8443/~_~/ames", "bridge URL")
	want := flag.String("hash", "", "expected certificate sha-256 (base64)")
	flag.Parse()

	wantRaw, err := base64.StdEncoding.DecodeString(*want)
	if err != nil {
		log.Fatalf("pincheck: bad -hash: %v", err)
	}

	d := webtransport.Dialer{
		TLSClientConfig: &tls.Config{
			InsecureSkipVerify: true, // we do our own SPKI pin below
			VerifyConnection: func(cs tls.ConnectionState) error {
				leaf := cs.PeerCertificates[0]
				got := sha256.Sum256(leaf.Raw)
				if string(got[:]) != string(wantRaw) {
					return errors.New("cert hash mismatch: got " +
						base64.StdEncoding.EncodeToString(got[:]))
				}
				return nil
			},
		},
	}
	_, sess, err := d.Dial(context.Background(), *url, nil)
	if err != nil {
		log.Fatalf("pincheck: dial: %v", err)
	}
	sess.CloseWithError(0, "")
	log.Printf("pincheck: OK — server accepted by cert-hash pin %s", *want)
}
