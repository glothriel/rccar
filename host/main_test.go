package main

import "testing"

func TestCommandLine(t *testing.T) {
	if got := string(commandLine('D', -255)); got != "D -255\n" {
		t.Fatalf("unexpected command: %q", got)
	}
}
