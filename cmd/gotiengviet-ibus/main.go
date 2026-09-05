package main

/*
#cgo pkg-config: ibus-1.0 glib-2.0 gio-2.0 gobject-2.0
#include <ibus.h>
#include <stdlib.h>
#include <string.h>
extern char* GoTransformTelex(char* input);
extern char* GoTransformVNI(char* input, int isVNI);
*/
import "C"
import (
	"unsafe"

	"github.com/isthaison/gotiengviet/engine"
)

//export GoTransformTelex
func GoTransformTelex(cstr *C.char) *C.char {
	goStr := C.GoString(cstr)
	out := engine.TransformStringTelex(goStr, true)
	return C.CString(out)
}

//export GoTransformVNI
func GoTransformVNI(cstr *C.char, isVNI C.int) *C.char {
	goStr := C.GoString(cstr)
	var out string
	if isVNI != 0 {
		out = engine.TransformStringVNI(goStr, true)
	} else {
		out = engine.TransformStringTelex(goStr, true)
	}
	return C.CString(out)
}

func main() {
	// Demo liên kết hệ thống: test pkg-config ibus-1.0
	// Build: go build -tags cgo -o gotiengviet-ibus ./cmd/gotiengviet-ibus
	// Chỉ dùng lib hệ thống: libibus-1.0.so, libglib-2.0.so, libgio-2.0.so
	// Không cần github.com/godbus/dbus, chỉ cần #cgo pkg-config: ibus-1.0

	// Ví dụ gọi qua CGO
	in := C.CString("chaof")
	defer C.free(unsafe.Pointer(in))
	out := GoTransformTelex(in)
	defer C.free(unsafe.Pointer(out))
	println("CGO Telex chaof ->", C.GoString(out))

	in2 := C.CString("tie6ng1")
	defer C.free(unsafe.Pointer(in2))
	out2 := GoTransformVNI(in2, 1)
	defer C.free(unsafe.Pointer(out2))
	println("CGO VNI tie6ng1 ->", C.GoString(out2))

	println("Engine core thuần Go stdlib + liên kết libibus hệ thống OK")
	println("Để chạy IBus đầy đủ, cần triển khai IBusEngine subclass bằng C API (ibus_engine_new, ibus_bus_register_component)")
	println("Xem ibus/gotiengviet.xml và ibus/engine.c mẫu (chỉ dùng ibus.h hệ thống)")
}
