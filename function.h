#ifndef FUNC_LECT_FUNCTION_H
#define FUNC_LECT_FUNCTION_H

#endif //FUNC_LECT_FUNCTION_H

struct bad_function_call : std::exception {
    [[nodiscard]] char const *what() const noexcept override {
        return "empty function call";
    }
};


template<typename T>
constexpr static bool is_small_storage =
        sizeof(T) <= sizeof(void *) && std::is_nothrow_move_constructible<T>::value && sizeof(void *) % alignof(T) == 0;

template<typename R, typename... Args>
struct storage;

template<typename R, typename... Args>
struct methods {
    typedef R (*invoke_fn_t)(storage<R, Args...> const *, Args...);

    typedef void (*deleter_fn_t)(storage<R, Args...> *);

    typedef void (*copy_fn_t)(storage<R, Args...> *, storage<R, Args...> const *);

    typedef void (*move_fn_t)(storage<R, Args...> *, storage<R, Args...> *);


    invoke_fn_t invoke;
    deleter_fn_t deleter;
    copy_fn_t copier;
    move_fn_t mover;
};

template<typename R, typename... Args>
methods<R, Args...> const *get_empty_methods() {
    static constexpr methods<R, Args...> table = {
            [](storage<R, Args...> const *, Args...) -> R {
                throw bad_function_call();
            },
            [](storage<R, Args...> *) {

            },
            [](storage<R, Args...> *dest, storage<R, Args...> const *src) {
                dest->methods_lst = src->methods_lst;
            },
            [](storage<R, Args...> *dest, storage<R, Args...> *src) {
                dest->methods_lst = std::move(src->methods_lst);
            }

    };
    return &table;
}

template<typename T, bool isSmall>
struct object_traits;

template<typename R, typename... Args>
struct storage {
    storage() noexcept: obj(), methods_lst(get_empty_methods<R, Args...>()) {}

    template<typename T>
    explicit
    storage(T val) : obj(), methods_lst(object_traits<T, is_small_storage<T>>::template get_methods<R, Args...>()) {}

    storage(storage const &other) {
        other.methods_lst->copier(this, &other);
    }

    storage(storage &&other) noexcept {
        other.methods_lst->mover(this, &other);
    }

    ~storage() {
        methods_lst->deleter(this);
    }

    storage &operator=(storage const &other) {
        if (this != &other) {
            storage(other).swap(*this);
        }
        return *this;
    }

    storage &operator=(storage &&other) noexcept {
        if (this != &other) {
            methods_lst->deleter(this);
            other.methods_lst->mover(this, &other);
        }
        return *this;
    }

    void swap(storage &other) {
        std::swap(obj, other.obj);
        std::swap(methods_lst, other.methods_lst);
    }

    typename std::aligned_storage<sizeof(void *), alignof(void *)>::type obj;

    methods<R, Args...> const *methods_lst;
};


template<typename T>
struct object_traits<T, true> {
    template<typename R, typename... Args>
    static methods<R, Args...> const *get_methods() {
        static constexpr methods<R, Args...> table{
                [](storage<R, Args...> const *src, Args... args) -> R {
                    return (reinterpret_cast<const T &>(src->obj))(std::forward<Args>(args)...);
                },
                [](storage<R, Args...> *src) {
                    get_target(src)->~T();
                    src->methods_lst = get_empty_methods<R, Args...>();
                },
                [](storage<R, Args...> *dest, storage<R, Args...> const *src) {
                    new(&dest->obj) T(reinterpret_cast<const T &>(src->obj));
                    dest->methods_lst = src->methods_lst;
                },
                [](storage<R, Args...> *dest, storage<R, Args...> *src) {
                    new(&dest->obj) T(std::move(*reinterpret_cast<T const *>(src)));
                    dest->methods_lst = src->methods_lst;
                    src->methods_lst = get_empty_methods<R, Args...>();
                }
        };
        return &table;
    }

    template<typename R, typename... Args>
    static T const *get_target(const storage<R, Args...> *stg) {
        return &reinterpret_cast<const T &>(stg->obj);
    }
};

template<typename T>
struct object_traits<T, false> {
    template<typename R, typename... Args>
    static methods<R, Args...> const *get_methods() {
        static constexpr methods<R, Args...> table{
                [](storage<R, Args...> const *src, Args... args) {
                    return (*get_target(src))(std::forward<Args>(args)...);
                },
                [](storage<R, Args...> *src) {
                    delete get_target(src);
                    src->methods_lst = get_empty_methods<R, Args...>();
                },
                [](storage<R, Args...> *dest, storage<R, Args...> const *src) {
                    reinterpret_cast<void *&>(dest->obj) = new T(*get_target(src));
                    dest->methods_lst = src->methods_lst;
                },
                [](storage<R, Args...> *dest, storage<R, Args...> *src) {
                    reinterpret_cast<void *&>(dest->obj) = const_cast<T *>(get_target(
                            src)); // todo maybe should use target func
                    dest->methods_lst = src->methods_lst;
                    src->methods_lst = get_empty_methods<R, Args...>();
                }
        };
        return &table;
    }

    template<typename R, typename... Args>
    static T const *get_target(const storage<R, Args...> *stg) {
        return static_cast<T const *>(reinterpret_cast<void *const &>(stg->obj));
    }
};


template<typename F>
struct function;

template<typename R, typename... Args>
struct function<R(Args...)> {
public:
    function() noexcept {
        stg.methods_lst = get_empty_methods<R, Args...>();
    };

    function(function const &other) {
        other.stg.methods_lst->copier(&this->stg, &other.stg);
    };

    function(function &&other) noexcept {
        other.stg.methods_lst->mover(&this->stg, &other.stg);
    };

    template<typename T>
    function(T val) {
        stg.methods_lst = object_traits<T, is_small_storage<T>>::template get_methods<R, Args...>();
        if (is_small_storage<T>) {
            new(&stg.obj) T(std::move(val));
        } else {
            reinterpret_cast<void *&>(stg.obj) = new T(std::move(val));
        }

    };

    function &operator=(function const &rhs) {
        this->stg = rhs.stg; // todo спросить почему я не могу это сделать по человечески
        return *this;
    };

    function &operator=(function &&rhs) noexcept = default;


    ~function() = default;

    explicit operator bool() const noexcept {
        return stg.methods_lst != get_empty_methods<R, Args...>();
    };

    R operator()(Args... args) const {
        return stg.methods_lst->invoke(&stg, std::forward<Args>(args)...);
    };

    template<typename T>
    T *target() noexcept {
        if (stg.methods_lst != object_traits<T, is_small_storage<T>>::template get_methods<R, Args...>()) {
            return nullptr;
        }
        return const_cast<T *>(object_traits<T, is_small_storage<T>>::template get_target<R, Args...>(&this->stg));
    };

    template<typename T>
    T const *target() const noexcept {
        if (stg.methods_lst != object_traits<T, is_small_storage<T>>::template get_methods<R, Args...>()) {
            return nullptr;
        }
        return object_traits<T, is_small_storage<T>>::template get_target<R, Args...>(&this->stg);
    };

private:
    storage<R, Args...> stg;
};
