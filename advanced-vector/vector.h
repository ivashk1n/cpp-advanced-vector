/* Разместите здесь код класса Vector*/
#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <type_traits>


//Шаблонный класс RawMemory будет отвечать за хранение буфера, который вмещает заданное количество элементов, и предоставлять доступ к элементам по индексу
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
        buffer_ = other.buffer_;
        capacity_ = other.capacity_;
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
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

    size_t Capacity() const {
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

    //конструктор
     explicit Vector(size_t size)
        : data_(size)
        , size_(size)  
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    // конструктор копирования
    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  
    {    
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(std::exchange(other.size_, 0)) {}

    // Присвоение без выделения дополнительной вместимости
    void CopyAssignNoRealloc(const Vector& rhs) {

        size_t common = std::min(size_, rhs.size_);
        std::copy(rhs.data_.GetAddress(), rhs.data_ + common, data_.GetAddress());
        
        if (size_ > rhs.size_) {
            std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
        } else {
            std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
        }

        size_ = rhs.size_;
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector tmp(rhs);      // может выбросить исключение
                Swap(tmp);            // безопасно, noexcept
            } else {
               CopyAssignNoRealloc(rhs);
            }
        }
        return *this;
    }


    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    void Swap(Vector& other) noexcept{
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
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

    //В константном операторе [] используется оператор  const_cast, чтобы снять константность с ссылки на текущий объект и вызвать неконстантную версию оператора []
    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        
        // проверка на перемещяемость или на возможность копирования 
        if constexpr (std::is_nothrow_move_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else if constexpr (std::is_copy_constructible_v<T>) {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else if constexpr (std::is_move_constructible_v<T>) {
            // Перемещение с возможными исключениями — тоже допустимо
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>,
                        "Type T must be either move or copy constructible");
        }
            
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        else {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    
    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    void PopBack() /* noexcept */ {
        if (size_ > 0) {
            std::destroy_at(data_.GetAddress() + size_ - 1);
            --size_;
        }
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args){
        if (size_ == Capacity()) {

            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + size_) T(std::forward<Args>(args)...);

            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                        std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());   
                }
                else {
                    std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
                }
            } catch (...) {
                std::destroy_at(new_data + size_);
                throw;
            }   
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else{
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;

        return data_[size_ - 1];
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert(pos >= begin() && pos < end());  // Убедимся, что позиция корректна
        size_t index = pos - begin();  // Индекс позиции вставки

        if (size_ == Capacity()) {
            // --- Случай 1: требуется перевыделение памяти ---
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            T* new_pos = new_data.GetAddress() + index;

            try {
                // Копируем все элементы до позиции вставки
                std::uninitialized_move_n(data_.GetAddress(), index, new_data.GetAddress());

                // Вставляем новый элемент на нужную позицию
                new (new_pos) T(std::forward<Args>(args)...);

                // Копируем все элементы после позиции вставки
                std::uninitialized_move_n(data_.GetAddress() + index, size_ - index, new_pos + 1);
            } catch (...) {
                // Если что-то пошло не так — уничтожаем всё скопированное
                std::destroy_n(new_data.GetAddress(), index);
                throw;
            }

            // Удаляем старые объекты и меняем буфер
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            ++size_;
            return begin() + index;
        } else {
            // --- Случай 2: памяти хватает, вставка "на месте" ---

            T tmp(std::forward<Args>(args)...);

            // Если size_ != 0, то можно сдвигать
            if (size_ != 0) {
                // Конструируем копию последнего элемента
                new (data_ + size_) T(std::move(data_[size_ - 1]));

                try {
                    // Сдвигаем диапазон [index, size_ - 1] вправо на 1
                    std::move_backward(data_ + index, data_ + size_ - 1, data_ + size_);
                    // Вставляем временный объект
                    data_[index] = std::move(tmp);
                } catch (...) {
                    std::destroy_at(data_ + size_);
                    throw;
                }
            }
            else{ // Если вектор пуст создаём новый объект "на месте"
                new (data_ + index) T(std::forward<Args>(args)...);
            }

            ++size_;
            return begin() + index;
        }
    }

    iterator Insert(const_iterator pos, const T& value){
        return Emplace(pos, value);  // копируем value
    }

    iterator Insert(const_iterator pos, T&& value){
        return Emplace(pos, std::move(value));  // перемещаем value
    }


    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/{
        assert(pos >= begin() && pos < end());  // Убедимся, что позиция корректна

        iterator nonconst_pos = const_cast<iterator>(pos);
        std::move(nonconst_pos + 1, end(), nonconst_pos);  // Сдвигаем элементы влево
        std::destroy_at(data_ + --size_);                  // Уничтожаем последний элемент

        return nonconst_pos;
    }
    

private:
    
    RawMemory<T> data_;
    size_t size_ = 0;
};