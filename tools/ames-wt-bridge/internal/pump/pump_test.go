package pump

import (
	"bytes"
	"context"
	"errors"
	"io"
	"testing"
	"time"
)

type writeCloserFunc struct {
	bytes.Buffer
	closed int
}

func (w *writeCloserFunc) Close() error {
	w.closed++
	return nil
}

func TestSendPacketFraming(t *testing.T) {
	pkt := []byte("ames packet")
	datagramFull := errors.New("datagram full")

	tests := []struct {
		name        string
		datagramErr error
		openErr     error
		wantDgrams  int
		wantStreams int
	}{
		{
			name:       "datagram",
			wantDgrams: 1,
		},
		{
			name:        "stream fallback",
			datagramErr: datagramFull,
			wantDgrams:  1,
			wantStreams: 1,
		},
		{
			name:        "stream open failure drops",
			datagramErr: datagramFull,
			openErr:     errors.New("closed"),
			wantDgrams:  1,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			var dgrams [][]byte
			var streams []*writeCloserFunc

			sendDatagram := func(got []byte) error {
				dgrams = append(dgrams, append([]byte(nil), got...))
				return tt.datagramErr
			}
			openUni := func(context.Context) (io.WriteCloser, error) {
				if tt.openErr != nil {
					return nil, tt.openErr
				}
				str := &writeCloserFunc{}
				streams = append(streams, str)
				return str, nil
			}

			sendPacket(context.Background(), "test", sendDatagram, openUni, pkt)

			if len(dgrams) != tt.wantDgrams {
				t.Fatalf("sent %d datagrams, want %d", len(dgrams), tt.wantDgrams)
			}
			for _, got := range dgrams {
				if !bytes.Equal(got, pkt) {
					t.Fatalf("datagram = %q, want %q", got, pkt)
				}
			}
			if len(streams) != tt.wantStreams {
				t.Fatalf("opened %d streams, want %d", len(streams), tt.wantStreams)
			}
			for _, got := range streams {
				if !bytes.Equal(got.Bytes(), pkt) {
					t.Fatalf("stream payload = %q, want %q", got.Bytes(), pkt)
				}
				if got.closed != 1 {
					t.Fatalf("stream closed %d times, want 1", got.closed)
				}
			}
		})
	}
}

func TestReceiveToFuncDatagram(t *testing.T) {
	want := []byte("datagram packet")
	calls := 0

	acceptUni := func(context.Context) (io.Reader, error) {
		return nil, errors.New("closed")
	}
	receiveDatagram := func(context.Context) ([]byte, error) {
		if calls != 0 {
			return nil, errors.New("closed")
		}
		calls++
		return append([]byte(nil), want...), nil
	}

	var got [][]byte
	receiveToFunc(context.Background(), "test", acceptUni, receiveDatagram, func(pkt []byte) {
		got = append(got, append([]byte(nil), pkt...))
	})

	if len(got) != 1 {
		t.Fatalf("delivered %d packets, want 1", len(got))
	}
	if !bytes.Equal(got[0], want) {
		t.Fatalf("packet = %q, want %q", got[0], want)
	}
}

func TestReceiveToFuncStream(t *testing.T) {
	want := []byte("stream packet")
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	accepted := false
	acceptUni := func(context.Context) (io.Reader, error) {
		if accepted {
			return nil, errors.New("closed")
		}
		accepted = true
		return bytes.NewReader(want), nil
	}
	receiveDatagram := func(ctx context.Context) ([]byte, error) {
		<-ctx.Done()
		return nil, ctx.Err()
	}

	delivered := make(chan []byte, 1)
	done := make(chan struct{})
	go func() {
		defer close(done)
		receiveToFunc(ctx, "test", acceptUni, receiveDatagram, func(pkt []byte) {
			delivered <- append([]byte(nil), pkt...)
			cancel()
		})
	}()

	select {
	case got := <-delivered:
		if !bytes.Equal(got, want) {
			t.Fatalf("packet = %q, want %q", got, want)
		}
	case <-time.After(time.Second):
		t.Fatal("stream packet was not delivered")
	}

	select {
	case <-done:
	case <-time.After(time.Second):
		t.Fatal("receive loop did not exit")
	}
}
