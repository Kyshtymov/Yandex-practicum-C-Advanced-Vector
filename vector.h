#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept: buffer_(other.buffer_), capacity_(other.capacity_){
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs)
            Swap(rhs);
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};


template <typename T>
class Vector {
public:

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept{
        return data_.GetAddress();
    }
    iterator end() noexcept{
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept{
        return data_.GetAddress();
    }
    const_iterator end() const noexcept{
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept{
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept{
        return data_.GetAddress() + size_;
    }

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(other.size_) {
        other.size_ = 0;
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector tmp(rhs);
                Swap(tmp);
            } else
                CopyElementsFromVector(rhs);
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_.Swap(rhs.data_);
            std::swap(size_, rhs.size_);
        }
        return *this;
    }

    void Swap(Vector& v) noexcept{
        this->data_.Swap(v.data_);
        std::swap(size_, v.size_);
    }

    void Resize(size_t new_size){
        if (new_size < size_)
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        else{
            if (new_size > data_.Capacity()) {
                Reserve(new_size);
            }
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }

        size_ = new_size;
    }

    void PushBack(const T& value){
        EmplaceBack(value);
    }

    void PushBack(T&& value){
        EmplaceBack(std::move(value));
    }

    void PopBack(){
        if (size_ != 0){
            std::destroy_n(data_.GetAddress() + size_ - 1, 1);
            size_--;
        }
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        InitializeAndDestroy(data_, new_data, size_);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args){
        assert(pos >= begin() && pos <= end());
        size_t index = pos - begin();
        if (size_ < data_.Capacity()){

            T temp(std::forward<Args>(args)...);

            new (data_.GetAddress() + size_) T(std::move(data_[size_ - 1]));

            try{
                std::move_backward(begin() + index, begin() + size_ - 1, begin() + size_);
            }
            catch(...){
                data_[size_].~T();
                throw;
            }

            data_[index] = std::move(temp);
            size_++;
            return data_.GetAddress() + index;
        }
        else{
            size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
            RawMemory<T> new_data(new_capacity);

            size_t new_constructed = 0;

            T* new_element;
            try {
                new_element = new (new_data.GetAddress() + index) T(std::forward<Args>(args)...);
                new_constructed++;

                InitializeObjects(data_.GetAddress(), new_data.GetAddress(), index);
                new_constructed += index;

                InitializeObjects(begin() + index, new_data.GetAddress() + index + 1, size_ - index);
                new_constructed += (size_ - index);
            }
            catch (...) {

                if (new_constructed > 1){
                    std::destroy_n(new_data.GetAddress(), new_constructed);
                }
                else{
                    if (new_constructed == 1)
                        std::destroy_at(new_element);
                }
                throw;
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            size_++;
            return new_element;
        }
    }

    iterator Erase(const_iterator pos){
        assert(pos >= begin() && pos <= end());
        size_t index = pos - begin();
        std::move(begin() + index + 1, end(), data_.GetAddress() + index);
        PopBack();
        return data_.GetAddress() + index;
    }

    iterator Insert(const_iterator pos, const T& value){
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value){
        return Emplace(pos, std::forward<T>(value));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args){
        if (size_ < data_.Capacity()){
            T* new_element = new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
            size_++;
            return *new_element;
        }
        else{
            size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
            RawMemory<T> new_data(new_capacity);

            T* new_element = new (new_data.GetAddress() + size_) T(std::forward<Args>(args)...);

            try{
                InitializeAndDestroy(data_, new_data, size_);
            }
            catch(...){
                new_element->~T();
                throw;
            }

            size_++;
            return *new_element;
        }
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:

    void CopyElementsFromVector(const Vector& rhs){

        size_t min_size = std::min(size_, rhs.size_);
        std::copy(rhs.begin(), rhs.begin() + min_size, begin());

        if (rhs.size_ > size_) {
            std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
        }
        else if (size_ > rhs.size_) {
            std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
        }
        size_ = rhs.size_;
    }

    void InitializeObjects(iterator data_iter, iterator new_data_iter, size_t size){
        if constexpr (std::is_nothrow_move_constructible_v<T> ||!std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_iter, size, new_data_iter);
        } else {
            std::uninitialized_copy_n(data_iter, size, new_data_iter);
        }
    }

    void InitializeAndDestroy(RawMemory<T>& data, RawMemory<T>& new_data, size_t size){
        InitializeObjects(data.GetAddress(), new_data.GetAddress(), size);
        std::destroy_n(data.GetAddress(), size);
        data.Swap(new_data);
    }

    RawMemory<T> data_;
    size_t size_ = 0;
};
