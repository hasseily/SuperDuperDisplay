#pragma once
#ifndef BYTEBUFFER_H
#define BYTEBUFFER_H

#include <iostream>
#include <memory>
#include <algorithm> // For std::copy
#include <cstring>   // For memcpy

class ByteBuffer {
public:
	ByteBuffer(size_t size) : size_(size), buffer_(new uint8_t[size], std::default_delete<uint8_t[]>()) {}

	// Prevent copy construction and assignment
	ByteBuffer(const ByteBuffer&) = delete;
	ByteBuffer& operator=(const ByteBuffer&) = delete;

	// Allow move construction and assignment
	ByteBuffer(ByteBuffer&&) noexcept = default;
	ByteBuffer& operator=(ByteBuffer&&) noexcept = default;

	// Copy data from a uint8_t* buffer
	void copyFrom(const uint8_t* source, size_t dest_index, size_t size) {
		if (size > (size_ - dest_index)) throw std::runtime_error("Source size exceeds buffer capacity.");
		std::copy(source, source + size, buffer_.get() + dest_index);
	}

	// Copy data to a uint8_t* buffer
	void copyTo(uint8_t* destination, size_t source_index, size_t size) const {
		if (size > size_) throw std::runtime_error("Destination size exceeds buffer capacity.");
		std::copy(buffer_.get() + source_index, buffer_.get() + source_index + size, destination);
	}

	// Getters
	uint8_t* data() { return buffer_.get(); }
	const uint8_t* data() const { return buffer_.get(); }
	size_t size() const { return size_; }

private:
	size_t size_;
	std::unique_ptr<uint8_t[]> buffer_;
};

#endif // BYTEBUFFER_H