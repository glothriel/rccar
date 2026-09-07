package main

import (
	"flag"
	"fmt"
	"os"
	"os/signal"
	"syscall"
	"time"

	"go.bug.st/serial"
)

func commandLine(kind byte, value int) []byte {
	return []byte(fmt.Sprintf("%c %d\n", kind, value))
}

func main() {
	portName := flag.String("port", "", "serial device, for example /dev/ttyACM0")
	drive := flag.Int("drive", 0, "drive command from -255 to 255")
	steering := flag.Int("steering", 0, "logical steering command from -100 to 100")
	flag.Parse()

	if *portName == "" || *drive < -255 || *drive > 255 || *steering < -100 || *steering > 100 {
		flag.Usage()
		os.Exit(2)
	}

	port, err := serial.Open(*portName, &serial.Mode{BaudRate: 115200})
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	defer port.Close()
	defer port.Write(commandLine('D', 0))

	stopping := make(chan os.Signal, 1)
	signal.Notify(stopping, os.Interrupt, syscall.SIGTERM)
	ticker := time.NewTicker(100 * time.Millisecond)
	defer ticker.Stop()

	fmt.Printf("drive=%d steering=%d; Ctrl-C stops\n", *drive, *steering)
	for {
		if _, err := port.Write(commandLine('D', *drive)); err != nil {
			fmt.Fprintln(os.Stderr, err)
			return
		}
		if _, err := port.Write(commandLine('S', *steering)); err != nil {
			fmt.Fprintln(os.Stderr, err)
			return
		}

		select {
		case <-stopping:
			return
		case <-ticker.C:
		}
	}
}
