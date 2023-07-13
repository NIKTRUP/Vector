#ifndef VECTOR_VECTOR_H
#define VECTOR_VECTOR_H
#include <cinttypes>
#include <cassert>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
            : buffer_(Allocate(capacity))
            , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;

    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Deallocate(buffer_);
            Swap(rhs);
        }
        return *this;
    }


    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
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

    [[nodiscard]] size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

/** Для ревьюера:
   Добрый день! Появилось пару вопросов:
 - Стоит ли разбить класс Vector на .hpp и .cpp файлы ? Если нет, то почему ? 
 - Стоит ли оставлять PushBack и EmplaceBack c передачей по forward ссылке,
    или надо бы разбить на передачу по rvalue-ссылке и константной ?
    P.S: Спасибо за работу !
 */

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size)
            : data_(size)
            , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }

    Vector(const Vector& other)
            : data_(other.size_)
            , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept :
    data_(std::move(other.data_)),
    size_(std::exchange(other.size_, 0))
    {}

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                std::copy_n(rhs.data_.GetAddress(), std::min(size_, rhs.size_), data_.GetAddress());

                if (rhs.size_ < size_) {
                    std::destroy_n(data_ + rhs.size_, size_ - rhs.size_);
                }
                else {
                    std::uninitialized_copy_n(rhs.data_ + size_, rhs.size_ - size_, data_ + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept{
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;

    }

    void Swap(Vector& other) noexcept{
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Resize(size_t new_size){
        if(new_size < size_){
            std::destroy_n(data_+ new_size, size_ - new_size);
        }else if(new_size > size_){
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    template <typename Arg>
    void PushBack(Arg&& value){
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + size_) T(std::forward<Arg>(value));
            TransferObjectsN(size_, new_data);

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }else{
            new (data_ + size_) T(std::forward<Arg>(value));
        }
        ++size_;
    }

    template<typename Arg>
    iterator Insert(const_iterator pos, Arg&& value){
        return Emplace(pos, std::forward<Arg>(value));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + size_) T(std::forward<Args>(args)...);
            TransferObjectsN(size_, new_data);

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }else{
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return data_[size_ - 1];
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args){

        if (pos == end()) {
            return &EmplaceBack(std::forward<Args>(args)...);
        }

        auto pos_idx = static_cast<size_t>(pos - begin());
        if(size_ == Capacity()){
            RawMemory<T> new_data{size_ == 0 ? 1 : size_ * 2};
            new (new_data + pos_idx) T(std::forward<Args>(args)...);
            try {
                TransferObjectsN(data_.GetAddress(), pos_idx, new_data.GetAddress());
            }  catch (...) {
                new_data[pos_idx].~T();
                throw;
            }

            try {
                TransferObjectsN(data_ + pos_idx, size_ - pos_idx, new_data + (pos_idx + 1));
            }  catch (...) {
                std::destroy_n(new_data.GetAddress(), pos_idx + 1);
                throw;
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);

        }else{
            T copy(std::forward<Args>(args)...);
            new (data_ + size_) T(std::move(data_[size_ - 1]));
            std::move_backward(begin() + pos_idx, end() - 1, end());
            data_[pos_idx] = std::move(copy);
        }
        ++size_;
        return begin() + pos_idx;
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        assert(size_ > 0);
        auto pos_idx = static_cast<size_t>(pos - begin());
        std::move(begin() + pos_idx + 1, end(), begin() + pos_idx);
        data_[size_ - 1].~T();
        --size_;
        return begin() + pos_idx;
    }

    iterator begin() noexcept{
        return data_.GetAddress();
    }

    iterator end() noexcept{
        return data_ + size_;
    }

    const_iterator begin() const noexcept{
        return data_.GetAddress();
    }
    const_iterator end() const noexcept{
        return data_ + size_;
    }

    const_iterator cbegin() const noexcept{
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept{
        return data_ + size_;
    }

    void PopBack() noexcept {
        std::destroy_at(data_ + size_ - 1);
        --size_;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        TransferObjectsN(size_, new_data);

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    [[nodiscard]] std::size_t Size() const noexcept {
        return size_;
    }

    [[nodiscard]] std::size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](std::size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](std::size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

private:

    void TransferObjectsN(T* data, std::size_t size, T* new_data) {
        if(size == 0){ return;}
        // Проверка на отсутствие копирующего конструктора или noexcept-конструктор перемещения во время компиляции.
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data, size, new_data);
        } else {
            std::uninitialized_copy_n(data, size, new_data);
        }
    }

    void TransferObjectsN(std::size_t size, RawMemory<T>& new_data){
        TransferObjectsN(data_.GetAddress(), size, new_data.GetAddress());
    }

    RawMemory<T> data_;
    std::size_t size_ = 0;
};


#endif //VECTOR_VECTOR_H
