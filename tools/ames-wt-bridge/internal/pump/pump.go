// Package pump moves ames packets between a WebTransport session and a UDP
// socket, in both directions.
//
// One ames packet maps to one QUIC datagram when it fits. A packet too large
// for the session's current max datagram size is sent instead as one
// ephemeral unidirectional stream (open, write, FIN) — reliability on that
// hop is harmless because ames packets are idempotent, and per-packet
// streams preserve out-of-order delivery between packets. See
// doc/spec/ames-over-quic.md §4.1 in the urbit repo.
package pump

import (
	"context"
	"encoding/hex"
	"io"
	"log"
	"net"

	"github.com/quic-go/quic-go"
	"github.com/quic-go/webtransport-go"
)

// Verbose enables per-packet hex logging (debugging and wire capture).
var Verbose bool

func trace(dir string, pkt []byte) {
	if Verbose {
		log.Printf("pump: %s %d bytes\n%s", dir, len(pkt), hex.Dump(pkt))
	}
}

// maxPacket bounds a single ames packet (old ames caps UDP payloads at
// 1472 bytes; mesa packets are of the same order). Streams carrying more
// than this are dropped as malformed.
const maxPacket = 65536

func sendPacket(ctx context.Context, label string, sendDatagram func([]byte) error, openUni func(context.Context) (io.WriteCloser, error), pkt []byte) {
	trace(label, pkt)
	if err := sendDatagram(pkt); err == nil {
		return
	}
	str, err := openUni(ctx)
	if err != nil {
		return // session closed; drop, ames retransmits
	}
	if _, err := str.Write(pkt); err != nil {
		_ = str.Close()
		return
	}
	if err := str.Close(); err != nil {
		log.Printf("pump: stream close: %v", err)
	}
}

func receiveToFunc(ctx context.Context, label string, acceptUni func(context.Context) (io.Reader, error), receiveDatagram func(context.Context) ([]byte, error), fn func(pkt []byte)) {
	go func() {
		for {
			str, err := acceptUni(ctx)
			if err != nil {
				return // session closed
			}
			go func() {
				pkt, err := io.ReadAll(io.LimitReader(str, maxPacket+1))
				if err != nil || len(pkt) == 0 || len(pkt) > maxPacket {
					return // truncated or oversized; drop
				}
				trace(label, pkt)
				fn(pkt)
			}()
		}
	}()
	for {
		pkt, err := receiveDatagram(ctx)
		if err != nil {
			return // session closed
		}
		trace(label, pkt)
		fn(pkt)
	}
}

// SendPacket sends one ames packet over sess: as a datagram if it fits,
// else as one unidirectional stream. Drops on session failure — loss is
// ames' problem, and ames already solves it.
func SendPacket(ctx context.Context, sess *webtransport.Session, pkt []byte) {
	sendPacket(ctx, "udp->wt", sess.SendDatagram, func(ctx context.Context) (io.WriteCloser, error) {
		return sess.OpenUniStreamSync(ctx)
	}, pkt)
}

// ReceiveToFunc delivers every packet arriving on sess — datagrams and
// per-packet unidirectional streams — to fn. Blocks until the session ends.
func ReceiveToFunc(ctx context.Context, sess *webtransport.Session, fn func(pkt []byte)) {
	receiveToFunc(ctx, "wt->udp", func(ctx context.Context) (io.Reader, error) {
		return sess.AcceptUniStream(ctx)
	}, sess.ReceiveDatagram, fn)
}

// SendQUICPacket sends one ames packet over a raw QUIC ames/1 connection.
func SendQUICPacket(ctx context.Context, conn *quic.Conn, pkt []byte) {
	sendPacket(ctx, "udp->quic", conn.SendDatagram, func(ctx context.Context) (io.WriteCloser, error) {
		return conn.OpenUniStreamSync(ctx)
	}, pkt)
}

// QUICReceiveToFunc delivers every packet arriving on a raw QUIC connection.
func QUICReceiveToFunc(ctx context.Context, conn *quic.Conn, fn func(pkt []byte)) {
	receiveToFunc(ctx, "quic->udp", func(ctx context.Context) (io.Reader, error) {
		return conn.AcceptUniStream(ctx)
	}, conn.ReceiveDatagram, fn)
}

// UDPToSession reads packets from conn and sends each over sess.
func UDPToSession(ctx context.Context, conn *net.UDPConn, sess *webtransport.Session) {
	buf := make([]byte, maxPacket)
	for {
		n, _, err := conn.ReadFromUDP(buf)
		if err != nil {
			return // socket closed
		}
		SendPacket(ctx, sess, buf[:n])
	}
}

// SessionToUDP delivers every packet from sess to the connected socket conn.
func SessionToUDP(ctx context.Context, sess *webtransport.Session, conn *net.UDPConn) {
	ReceiveToFunc(ctx, sess, func(pkt []byte) {
		if _, err := conn.Write(pkt); err != nil {
			log.Printf("pump: udp write: %v", err)
		}
	})
}

// UDPToQUIC reads packets from conn and sends each over a raw QUIC connection.
func UDPToQUIC(ctx context.Context, conn *net.UDPConn, qconn *quic.Conn) {
	buf := make([]byte, maxPacket)
	for {
		n, _, err := conn.ReadFromUDP(buf)
		if err != nil {
			return // socket closed
		}
		SendQUICPacket(ctx, qconn, buf[:n])
	}
}

// QUICToUDP delivers every packet from qconn to the connected socket conn.
func QUICToUDP(ctx context.Context, qconn *quic.Conn, conn *net.UDPConn) {
	QUICReceiveToFunc(ctx, qconn, func(pkt []byte) {
		if _, err := conn.Write(pkt); err != nil {
			log.Printf("pump: udp write: %v", err)
		}
	})
}
