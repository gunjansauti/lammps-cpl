//
// Created by Yury Lysogorskiy on 11.01.20.
//
#ifndef ACE_CONTIGUOUSARRAYND_H
#define ACE_CONTIGUOUSARRAYND_H

#include <string>

#include "ace_types.h"

using namespace std;

/**
 * Common predecessor class to represent multidimensional array of type T
 * and store it in memory contiguous form
 *
 * @tparam T data type
 */
template<typename T>
class ContiguousArrayND {
protected:
    T *data = nullptr; ///< pointer to contiguous data
    size_t size = 0; ///< total array size
    string array_name = "Array"; ///<array name
    bool is_proxy = false; ///< array is proxy (wrapper) and not owner of the memory
public:

    /**
     * Default empty constructor
     */
    ContiguousArrayND() = default;


    /**
     *  Constructor with array name
     * @param array_name name of array (for error logging)
     */
    ContiguousArrayND(string array_name) : array_name(array_name) {};

    /**
     * Copy constructor
     * @param other other ContiguousArrayND
     */
    ContiguousArrayND(const ContiguousArrayND &other) : array_name(other.array_name), size(other.size), is_proxy(other.is_proxy) {
#ifdef MULTIARRAY_LIFE_CYCLE
        cout<<array_name<<"::copy constructor"<<endl;
#endif
        if(!is_proxy) { //if not the proxy, then copy the values
            if (size > 0) {
                data = new T[size];
                for (size_t ind = 0; ind < size; ind++)
                    data[ind] = other.data[ind];
            }
        } else { //is proxy, then copy the pointer
            data = other.data;
        }
    }

    /**
     * Overload operator=
     * @param other another  ContiguousArrayND
     * @return itself
     */

    ContiguousArrayND &operator=(const ContiguousArrayND &other) {
#ifdef MULTIARRAY_LIFE_CYCLE
        cout<<array_name<<"::operator="<<endl;
#endif
        if (this != &other) {
            array_name = other.array_name;
            size = other.size;
            is_proxy = other.is_proxy;
            if(!is_proxy) { //if not the proxy, then copy the values
                if (size > 0) {

                    if(data!=nullptr) delete[] data;
                    data = new T[size];

                    for (size_t ind = 0; ind < size; ind++)
                        data[ind] = other.data[ind];
                }
            } else { //is proxy, then copy the pointer
                data = other.data;
            }
        }
        return *this;
    }


    //TODO: make destructor virtual, check the destructors in inherited classes

    /**
     * Destructor
     */
    ~ContiguousArrayND() {
#ifdef MULTIARRAY_LIFE_CYCLE
        cout<<array_name<<"::~destructor"<<endl;
#endif
        if(! is_proxy) {
            delete[] data;
        }
        data = nullptr;
    }

    /**
     * Set array name
     * @param name array name
     */
    void set_array_name(const string &name) {
        this->array_name = name;
    }

    /**
     * Get total number of elements in array (its size)
     * @return array size
     */
    size_t get_size() const {
        return size;
    }

    /**
     * Fill array with value
     * @param value value to fill
     */
    void fill(T value) {
        for (size_t ind = 0; ind < size; ind++)
            data[ind] = value;
    }

    /**
     * Get array data at absolute index ind for reading
     * @param ind absolute index
     * @return array value
     */
    inline const T &get_data(size_t ind) const {
#ifdef MULTIARRAY_INDICES_CHECK
        if ((ind < 0) | (ind >= size)) {
            printf("%s: get_data ind=%d out of range (0, %d)\n", array_name, ind, size);
            exit(EXIT_FAILURE);
        }
#endif
        return data[ind];
    }

    /**
     * Get array data at absolute index ind for writing
     * @param ind absolute index
     * @return array value
     */
    inline T &get_data(size_t ind) {
#ifdef MULTIARRAY_INDICES_CHECK
        if ((ind < 0) | (ind >= size)) {
            printf("%s: get_data ind=%d out of range (0, %d)\n", array_name, ind, size);
            exit(EXIT_FAILURE);
        }
#endif
        return data[ind];
    }

    /**
     * Get array data pointer
     * @return data array pointer
     */
    inline const T* get_data() const {
        return data;
    }

    /**
     * Overload comparison operator==
     * Compare the total size and array values elementwise.
     *
     * @param other another array
     * @return
     */
    bool operator==(const ContiguousArrayND &other) const {
        if (this->size != other.size)
            return false;

        for (size_t i = 0; i < this->size; ++i)
            if (this->data[i] != other.data[i])
                return false;

        return true;
    }


    /**
    * Convert to flatten vector<T> container
    * @return vector container
    */
    vector<T> to_flatten_vector() const {
        vector<T> res;

        res.resize(size);
        size_t vec_ind = 0;

        for (int vec_ind = 0; vec_ind < size; vec_ind++)
                res.at(vec_ind) = data[vec_ind];

        return res;
    } // end to_flatten_vector()


    /**
    * Set values from flatten vector<T> container
    * @param vec container
    */
    void set_flatten_vector(const vector<T> &vec) {
        if (vec.size() != size)
            throw std::invalid_argument("Flatten vector size is not consistent with expected size");
        for (size_t i = 0; i < size; i++) {
            data[i] = vec[i];
        }
    }

};


#endif //ACE_CONTIGUOUSARRAYND_H