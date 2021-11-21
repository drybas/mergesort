
#include <iostream>
#include <random>
#include <fstream>
#include <iterator>

#include <vector>
#include <map>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace chr = std::chrono;

template <typename>
class mview_iterator;

const size_t cPageSize = 4096;

template <typename T, typename Io, size_t PageSize>
class mview {
private:
    using internal_iterator = typename std::vector<T>::iterator;
public:
    using type = mview<T, Io, PageSize>;
    using value_type = T;
    using iterator = mview_iterator<type>;

    friend class mview_iterator<type>;

    mview(Io& io)
        : file_io_(io)
    {
        elems_count_ = io.size() / sizeof(T);
    }

    iterator begin() noexcept;
    iterator end() noexcept;

private:
    //using page_iterator = typename std::map<size_t, std::vector<T>>::iterator;
    std::shared_ptr<T[]> lock_interval(size_t offset) {
        std::cout << "lock_interval: " << offset << "\n";
        if (offset < elems_count_ * cPageSize) {
            std::shared_ptr<T[]> buffer(new T[cPageSize], [](T* buf) { delete [] buf; });
            auto read_bytes = file_io_.read(&buffer[0], cPageSize * sizeof(T), offset * sizeof(T));
            std::cout << "Bytes read: " << read_bytes << "\n";
            return buffer;
        }
        return nullptr;
    }
    T& get_elem(size_t offset);

private:
    size_t elems_count_;
    //std::map<size_t, std::shared_ptr<T[]>> pages_;
    //typedef typename std::vector<unsigned int> container_type;
    //typedef typename container_type::reference       reference;
    typedef std::shared_ptr<T[]> shared_buffer;
    Io file_io_;
};

template <typename T>
class mview_iterator {
public:
    using difference_type = std::ptrdiff_t;
    using value_type = typename T::value_type;
    using pointer = value_type*;
    using reference = value_type&;
    using iterator_category = std::random_access_iterator_tag;

    friend T;

    mview_iterator()
    {
        std::cout << "test \n";
    }

    bool operator ==(const mview_iterator<T>& rhs) const {
        return offset_ == rhs.offset_;
    }

    bool operator !=(const mview_iterator<T>& rhs) const {
        return !(*this == rhs);
    }

    bool operator <(const mview_iterator<T>& rhs) const {
        return offset_ < rhs.offset_;
    }

    reference operator*() const {
        if (range_ptr_ != nullptr) {
            return range_ptr_[offset_ % cPageSize];
        } else {
            return owner_->get_elem(offset_);
        }
   }

    mview_iterator<T>& operator++() {
        offset_++;
        if (offset_ == upper_bound_)
        {
            chunk_index_++;
            upper_bound_ += cPageSize;
            range_ptr_ = owner_->lock_interval(offset_);
        }

        return *this;
    }

    mview_iterator<T> operator++(int) {
        mview_iterator tmp(*this);
        offset_++;
        if (offset_ == upper_bound_)
        {
            chunk_index_++;
            upper_bound_ += cPageSize;
            range_ptr_ = owner_->lock_interval(offset_);
        }
        return tmp;
    }

    mview_iterator<T>& operator--() {
        offset_--;
        if (offset_ < (upper_bound_ - cPageSize))
        {
            chunk_index_--;
            range_ptr_ = owner_->lock_interval(offset_);
        }
        return *this;
    }

    mview_iterator<T> operator--(int) {
        mview_iterator tmp(*this, offset_);
        offset_--;
        if (offset_ < (upper_bound_ - cPageSize))
        {
            chunk_index_--;
            range_ptr_ = owner_->lock_interval(offset_);
        }
        return tmp;
    }

    difference_type operator-(const mview_iterator<T>& rhs) const {
        return offset_ - rhs.offset_;
    }

    difference_type operator+(const mview_iterator<T>& rhs) const {
        return offset_ + rhs.offset_;
    }

    mview_iterator<T>& operator+=(std::ptrdiff_t diff) {
        offset_ += diff;
        auto new_index = offset_ / cPageSize;
        if (new_index != chunk_index_) {
            chunk_index_ = new_index;
            upper_bound_ = (chunk_index_ + 1) * cPageSize;
            range_ptr_ = owner_->lock_interval(offset_);
        }
        return *this;
    }

private:
    mview_iterator(T* owner, size_t offset)
            : owner_(owner)
            , offset_(offset) {
        chunk_index_ = offset_ / cPageSize;
        upper_bound_ = (chunk_index_ + 1) * cPageSize;
        range_ptr_ = owner_->lock_interval(offset_);
    }
private:
    T* owner_ = nullptr;
    size_t offset_ = 0;
    size_t upper_bound_ = 0;
    size_t chunk_index_ = 0;

    //typename T::internal_iterator begin_;
    //typename T::internal_iterator end_;
    //std::weak_ptr<T[]> range_ptr_;
    std::shared_ptr<typename T::value_type[]> range_ptr_;
};

template<typename T>
mview_iterator<T> operator+(mview_iterator<T>& lhs, std::ptrdiff_t diff) {
    lhs += diff;
    return lhs;
}

template<typename T, typename Io, size_t PageSize>
typename mview<T, Io, PageSize>::iterator mview<T, Io, PageSize>::begin() noexcept {
    return iterator(this, 0);
}

template<typename T, typename Io, size_t PageSize>
typename mview<T, Io, PageSize>::iterator mview<T, Io, PageSize>::end() noexcept {
    return iterator(this, elems_count_);
}

template<typename T, typename Io, size_t PageSize>
T& mview<T, Io, PageSize>::get_elem(size_t offset) {
    //return value;
    throw std::logic_error("not implemented");
}

template<typename InputIt, typename OutputIt>
void merge(InputIt xs, InputIt xe, InputIt ys, InputIt ye, OutputIt zs) {
    while (xs != xe && ys != ye) {
        bool which = *ys < *xs;
        *zs++ = std::move(which ? *ys++ : *xs++);
    }

    std::move(xs, xe, zs);
    std::move(ys, ys, zs);
}

template<typename InputIt, typename OutputIt>
void merge_sort(InputIt xs, InputIt xe, OutputIt zs) {
    const size_t cutoff = 500;
    if (std::distance(xs, xe) <= cutoff) {
        std::stable_sort(xs, xe);
    } else {
        auto xm = xs + (xe - xs) / 2;
        auto zm = zs + (xm - xs);
        //auto ze = zs + (xe - xs) / 2; //?

        merge_sort(xs, xm, zs);
        merge_sort(xm, xe, zm);
        merge(xs, xm, xm, xe, zs);
    }
}

uint64_t generate_file(const char* filename, uint32_t size) {
    std::fstream out;
    out.open(filename, std::ios::binary | std::ios::out);
    if (!out.is_open())
        throw std::runtime_error("out file wasn't created");

    std::random_device rd;
    std::mt19937 gen(rd()); //Standard Mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<uint32_t> dis(0, std::numeric_limits<uint32_t>::max());

    auto total_size = 0;
    for (size_t piece = 0; piece < size; piece++) {
        std::vector<uint32_t> buffer(1024);
        for (size_t i = 0; i < buffer.size(); i++) {
            buffer[i] = dis(gen);
        }

        total_size += buffer.size();
        out.write(reinterpret_cast<char*>(&buffer[0]), sizeof(char) * buffer.size());
    }

    return total_size;
}

class ro_policy{
public:
    ro_policy(const std::string& file_name, bool ro) {
        if (ro) {
            if (stat(file_name.c_str(), &sb_) < 0)
                throw std::system_error(errno, std::generic_category());

            in_file_ = open(file_name.c_str(), O_RDONLY);
        }
        else {
            in_file_ = open(file_name.c_str(), O_RDWR | O_CREAT | O_TRUNC
                    ,  S_IROTH | S_IWOTH | S_IRGRP | S_IWGRP | S_IWUSR |  S_IRUSR);
        }

        if (in_file_ == -1)
            throw std::system_error(errno, std::generic_category());
    }

    ~ro_policy() {
        if (in_file_ != -1)
            close(in_file_);
    }

    ssize_t read(void* buffer, size_t size, off_t offset) {
        return pread(in_file_, buffer, size, offset);
    }
    void write() { }
    size_t size() const { return sb_.st_size; }

private:
    int in_file_;
    struct stat sb_;
};

void sort_file_st(const std::string& path) {
   ro_policy rp(path, true);
   mview<int, ro_policy, 4 * 1024 * 1024> in(rp);
   //mview<int, 4 * 1024 * 1024, rw_access_policy> out("out_test.a", false);
   //std::stable_sort(std::begin(in), std::end(in));
   std::fstream f("copy.a", std::ios::out);
   std::for_each(std::begin(in), std::end(in), [&f](auto e) {
       f.write(reinterpret_cast<char*>(&e), sizeof(e));
   });
   //merge_sort(std::begin(in), std::end(in), std::begin(out));
}

int main(int argc, const char* argv[]) {
    if (argc == 3 && std::strcmp(argv[1], "s") == 0) {
        /*auto written =*/ sort_file_st(argv[2]);
    } else if (argc == 4 && std::strcmp(argv[1], "g") == 0) {
        std::cout << "generate file ... \n";
        char *end = nullptr;
        auto bytes_written = generate_file(argv[3], std::strtol(argv[2], &end, 10));
    }
    else {
        std::cout << "Use following commands:\n"
                  << "\tg - generate file g size_kb file_path \n"
                  << "\ts - sort file s file_path \n"
                  << "\n";
    }

    return 0;
}
