#ifndef YARR_BASES_HPP
#define YARR_BASES_HPP

///@brief disables all special members
template <typename Derived>
class PureStatic
{
public:
    PureStatic()                              = delete;
    ~PureStatic()                             = delete;
    PureStatic(const PureStatic&)             = delete;
    PureStatic(PureStatic&&)                  = delete;
    PureStatic &operator =(const PureStatic&) = delete;
    PureStatic &operator =(PureStatic&&)      = delete;
};

///@brief enables getInstance function. Disables normal creation.
template <typename Derived>
class Singleton
{
public:
    static Derived& getInstance()
    {
        static Derived instance;
        return instance;
    }

protected:
    Singleton()  = default;
    ~Singleton() = default;

    Singleton(const Singleton&)             = delete;
    Singleton(Singleton&&)                  = delete;
    Singleton &operator =(const Singleton&) = delete;
    Singleton &operator =(Singleton&&)      = delete;
};

#endif // YARR_BASES_HPP
