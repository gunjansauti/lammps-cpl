
//automatically generate source code

#ifndef ACE_MULTIARRAY_H
#define ACE_MULTIARRAY_H

#include <cstring>
#include <stdexcept>
#include <vector>

#include "ace_contigous_array.h"

using namespace std;


template<class T>
class Array1D : public ContiguousArrayND<T> {
    using ContiguousArrayND<T>::array_name;
    using ContiguousArrayND<T>::data;
    using ContiguousArrayND<T>::size;

    // dimensions
    size_t dim[1] = {0};

    // strides
    size_t s[1] = {0};

    int ndim = 1;

public:

    // default empty constructor
    Array1D() = default;

    // parametrized constructor
    Array1D(const string &array_name) { this->array_name = array_name; }

    // parametrized constructor
    Array1D(size_t d0, const string &array_name = "Array1D") {
        init(d0, array_name);
    }

    //initialize array and strides
    void init(size_t d0, const string &array_name = "Array1D") {
        this->array_name = array_name;


        dim[0] = d0;

        s[0] = 1;

        if (size != s[0] * dim[0]) {
            size = s[0] * dim[0];
            if (data) delete[] data;
            data = new T[size];
            memset(data, 0, size * sizeof(T));
        } else {
            memset(data, 0, size * sizeof(T));
        }
    }

    void resize(size_t d0) {
        init(d0, this->array_name);
    }

    size_t get_dim(int d) const {
        return dim[d];
    }

#ifdef MULTIARRAY_INDICES_CHECK
    void check_indices(size_t i0) const {

        if((i0<0)|(i0>=dim[0])){
            printf("%s: index i0=%ld out of range (0, %ld)\n", array_name.c_str(), i0, dim[0]-1);
            exit(EXIT_FAILURE);
        }

    }
#endif

    inline const T &operator()(size_t i0) const {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0);
#endif

        return data[i0];

    }

    inline T &operator()(size_t i0) {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0);
#endif

        return data[i0];

    }

    bool operator==(const Array1D &other) const {
        //compare dimensions
        for (int d = 0; d < ndim; d++) {
            if (this->dim[d] != other.dim[d])
                return false;
        }
        return ContiguousArrayND<T>::operator==(other);
    }

    vector<T> to_vector() const {
        vector<T> res;

        res.resize(dim[0]);

        for (int i0 = 0; i0 < dim[0]; i0++) {
            res[i0] = operator()(i0);

        }


        return res;
    } // end to_vector()

    // parametrized constructor from vector
    Array1D(vector<T> vec, const string &array_name = "Array1D") {
        size_t d0 = 0;
        d0 = vec.size();


        init(d0, array_name);
        for (int i0 = 0; i0 < dim[0]; i0++) {
            operator()(i0) = vec.at(i0);

        }

    }

};

template<class T>
class Array2D : public ContiguousArrayND<T> {
    using ContiguousArrayND<T>::array_name;
    using ContiguousArrayND<T>::data;
    using ContiguousArrayND<T>::size;

    // dimensions
    size_t dim[2] = {0};

    // strides
    size_t s[2] = {0};

    int ndim = 2;

public:

    // default empty constructor
    Array2D() = default;

    // parametrized constructor
    Array2D(const string &array_name) { this->array_name = array_name; }

    // parametrized constructor
    Array2D(size_t d0, size_t d1, const string &array_name = "Array2D") {
        init(d0, d1, array_name);
    }

    //initialize array and strides
    void init(size_t d0, size_t d1, const string &array_name = "Array2D") {
        this->array_name = array_name;


        dim[0] = d0;
        dim[1] = d1;

        s[1] = 1;
        s[0] = s[1] * dim[1];

        if (size != s[0] * dim[0]) {
            size = s[0] * dim[0];
            if (data) delete[] data;
            data = new T[size];
            memset(data, 0, size * sizeof(T));
        } else {
            memset(data, 0, size * sizeof(T));
        }
    }

    void resize(size_t d0, size_t d1) {
        init(d0, d1, this->array_name);
    }

    size_t get_dim(int d) const {
        return dim[d];
    }

#ifdef MULTIARRAY_INDICES_CHECK
    void check_indices(size_t i0, size_t i1) const {

        if((i0<0)|(i0>=dim[0])){
            printf("%s: index i0=%ld out of range (0, %ld)\n", array_name.c_str(), i0, dim[0]-1);
            exit(EXIT_FAILURE);
        }

        if((i1<0)|(i1>=dim[1])){
            printf("%s: index i1=%ld out of range (0, %ld)\n", array_name.c_str(), i1, dim[1]-1);
            exit(EXIT_FAILURE);
        }

    }
#endif

    inline const T &operator()(size_t i0, size_t i1) const {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1);
#endif

        return data[i0 * s[0] + i1];

    }

    inline T &operator()(size_t i0, size_t i1) {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1);
#endif

        return data[i0 * s[0] + i1];

    }

    bool operator==(const Array2D &other) const {
        //compare dimensions
        for (int d = 0; d < ndim; d++) {
            if (this->dim[d] != other.dim[d])
                return false;
        }
        return ContiguousArrayND<T>::operator==(other);
    }

    vector<vector<T>> to_vector() const {
        vector<vector<T>> res;

        res.resize(dim[0]);

        for (int i0 = 0; i0 < dim[0]; i0++) {
            res[i0].resize(dim[1]);


            for (int i1 = 0; i1 < dim[1]; i1++) {
                res[i0][i1] = operator()(i0, i1);

            }
        }


        return res;
    } // end to_vector()

    // parametrized constructor from vector
    Array2D(vector<vector<T>> vec, const string &array_name = "Array2D") {
        size_t d0 = 0;
        size_t d1 = 0;
        d0 = vec.size();

        if (d0 > 0) {
            d1 = vec.at(0).size();


        }

        init(d0, d1, array_name);
        for (int i0 = 0; i0 < dim[0]; i0++) {
            if (vec.at(i0).size() != d1)
                throw std::invalid_argument("Vector size is not constant at dimension 1");

            for (int i1 = 0; i1 < dim[1]; i1++) {
                operator()(i0, i1) = vec.at(i0).at(i1);

            }
        }

    }

};

template<class T>
class Array3D : public ContiguousArrayND<T> {
    using ContiguousArrayND<T>::array_name;
    using ContiguousArrayND<T>::data;
    using ContiguousArrayND<T>::size;

    // dimensions
    size_t dim[3] = {0};

    // strides
    size_t s[3] = {0};

    int ndim = 3;

public:

    // default empty constructor
    Array3D() = default;

    // parametrized constructor
    Array3D(const string &array_name) { this->array_name = array_name; }

    // parametrized constructor
    Array3D(size_t d0, size_t d1, size_t d2, const string &array_name = "Array3D") {
        init(d0, d1, d2, array_name);
    }

    //initialize array and strides
    void init(size_t d0, size_t d1, size_t d2, const string &array_name = "Array3D") {
        this->array_name = array_name;


        dim[0] = d0;
        dim[1] = d1;
        dim[2] = d2;

        s[2] = 1;
        s[1] = s[2] * dim[2];
        s[0] = s[1] * dim[1];

        if (size != s[0] * dim[0]) {
            size = s[0] * dim[0];
            if (data) delete[] data;
            data = new T[size];
            memset(data, 0, size * sizeof(T));
        } else {
            memset(data, 0, size * sizeof(T));
        }
    }

    void resize(size_t d0, size_t d1, size_t d2) {
        init(d0, d1, d2, this->array_name);
    }

    size_t get_dim(int d) const {
        return dim[d];
    }

#ifdef MULTIARRAY_INDICES_CHECK
    void check_indices(size_t i0, size_t i1, size_t i2) const {

        if((i0<0)|(i0>=dim[0])){
            printf("%s: index i0=%ld out of range (0, %ld)\n", array_name.c_str(), i0, dim[0]-1);
            exit(EXIT_FAILURE);
        }

        if((i1<0)|(i1>=dim[1])){
            printf("%s: index i1=%ld out of range (0, %ld)\n", array_name.c_str(), i1, dim[1]-1);
            exit(EXIT_FAILURE);
        }

        if((i2<0)|(i2>=dim[2])){
            printf("%s: index i2=%ld out of range (0, %ld)\n", array_name.c_str(), i2, dim[2]-1);
            exit(EXIT_FAILURE);
        }

    }
#endif

    inline const T &operator()(size_t i0, size_t i1, size_t i2) const {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1, i2);
#endif

        return data[i0 * s[0] + i1 * s[1] + i2];

    }

    inline T &operator()(size_t i0, size_t i1, size_t i2) {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1, i2);
#endif

        return data[i0 * s[0] + i1 * s[1] + i2];

    }

    bool operator==(const Array3D &other) const {
        //compare dimensions
        for (int d = 0; d < ndim; d++) {
            if (this->dim[d] != other.dim[d])
                return false;
        }
        return ContiguousArrayND<T>::operator==(other);
    }

    vector<vector<vector<T>>> to_vector() const {
        vector<vector<vector<T>>> res;

        res.resize(dim[0]);

        for (int i0 = 0; i0 < dim[0]; i0++) {
            res[i0].resize(dim[1]);


            for (int i1 = 0; i1 < dim[1]; i1++) {
                res[i0][i1].resize(dim[2]);


                for (int i2 = 0; i2 < dim[2]; i2++) {
                    res[i0][i1][i2] = operator()(i0, i1, i2);

                }
            }
        }


        return res;
    } // end to_vector()

    // parametrized constructor from vector
    Array3D(vector<vector<vector<T>>> vec, const string &array_name = "Array3D") {
        size_t d0 = 0;
        size_t d1 = 0;
        size_t d2 = 0;
        d0 = vec.size();

        if (d0 > 0) {
            d1 = vec.at(0).size();
            if (d1 > 0) {
                d2 = vec.at(0).at(0).size();


            }
        }

        init(d0, d1, d2, array_name);
        for (int i0 = 0; i0 < dim[0]; i0++) {
            if (vec.at(i0).size() != d1)
                throw std::invalid_argument("Vector size is not constant at dimension 1");

            for (int i1 = 0; i1 < dim[1]; i1++) {
                if (vec.at(i0).at(i1).size() != d2)
                    throw std::invalid_argument("Vector size is not constant at dimension 2");

                for (int i2 = 0; i2 < dim[2]; i2++) {
                    operator()(i0, i1, i2) = vec.at(i0).at(i1).at(i2);

                }
            }
        }

    }

};

template<class T>
class Array4D : public ContiguousArrayND<T> {
    using ContiguousArrayND<T>::array_name;
    using ContiguousArrayND<T>::data;
    using ContiguousArrayND<T>::size;

    // dimensions
    size_t dim[4] = {0};

    // strides
    size_t s[4] = {0};

    int ndim = 4;

public:

    // default empty constructor
    Array4D() = default;

    // parametrized constructor
    Array4D(const string &array_name) { this->array_name = array_name; }

    // parametrized constructor
    Array4D(size_t d0, size_t d1, size_t d2, size_t d3, const string &array_name = "Array4D") {
        init(d0, d1, d2, d3, array_name);
    }

    //initialize array and strides
    void init(size_t d0, size_t d1, size_t d2, size_t d3, const string &array_name = "Array4D") {
        this->array_name = array_name;


        dim[0] = d0;
        dim[1] = d1;
        dim[2] = d2;
        dim[3] = d3;

        s[3] = 1;
        s[2] = s[3] * dim[3];
        s[1] = s[2] * dim[2];
        s[0] = s[1] * dim[1];

        if (size != s[0] * dim[0]) {
            size = s[0] * dim[0];
            if (data) delete[] data;
            data = new T[size];
            memset(data, 0, size * sizeof(T));
        } else {
            memset(data, 0, size * sizeof(T));
        }
    }

    void resize(size_t d0, size_t d1, size_t d2, size_t d3) {
        init(d0, d1, d2, d3, this->array_name);
    }

    size_t get_dim(int d) const {
        return dim[d];
    }

#ifdef MULTIARRAY_INDICES_CHECK
    void check_indices(size_t i0, size_t i1, size_t i2, size_t i3) const {

        if((i0<0)|(i0>=dim[0])){
            printf("%s: index i0=%ld out of range (0, %ld)\n", array_name.c_str(), i0, dim[0]-1);
            exit(EXIT_FAILURE);
        }

        if((i1<0)|(i1>=dim[1])){
            printf("%s: index i1=%ld out of range (0, %ld)\n", array_name.c_str(), i1, dim[1]-1);
            exit(EXIT_FAILURE);
        }

        if((i2<0)|(i2>=dim[2])){
            printf("%s: index i2=%ld out of range (0, %ld)\n", array_name.c_str(), i2, dim[2]-1);
            exit(EXIT_FAILURE);
        }

        if((i3<0)|(i3>=dim[3])){
            printf("%s: index i3=%ld out of range (0, %ld)\n", array_name.c_str(), i3, dim[3]-1);
            exit(EXIT_FAILURE);
        }

    }
#endif

    inline const T &operator()(size_t i0, size_t i1, size_t i2, size_t i3) const {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1, i2, i3);
#endif

        return data[i0 * s[0] + i1 * s[1] + i2 * s[2] + i3];

    }

    inline T &operator()(size_t i0, size_t i1, size_t i2, size_t i3) {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1, i2, i3);
#endif

        return data[i0 * s[0] + i1 * s[1] + i2 * s[2] + i3];

    }

    bool operator==(const Array4D &other) const {
        //compare dimensions
        for (int d = 0; d < ndim; d++) {
            if (this->dim[d] != other.dim[d])
                return false;
        }
        return ContiguousArrayND<T>::operator==(other);
    }

    vector<vector<vector<vector<T>>>> to_vector() const {
        vector<vector<vector<vector<T>>>> res;

        res.resize(dim[0]);

        for (int i0 = 0; i0 < dim[0]; i0++) {
            res[i0].resize(dim[1]);


            for (int i1 = 0; i1 < dim[1]; i1++) {
                res[i0][i1].resize(dim[2]);


                for (int i2 = 0; i2 < dim[2]; i2++) {
                    res[i0][i1][i2].resize(dim[3]);


                    for (int i3 = 0; i3 < dim[3]; i3++) {
                        res[i0][i1][i2][i3] = operator()(i0, i1, i2, i3);

                    }
                }
            }
        }


        return res;
    } // end to_vector()

    // parametrized constructor from vector
    Array4D(vector<vector<vector<vector<T>>>> vec, const string &array_name = "Array4D") {
        size_t d0 = 0;
        size_t d1 = 0;
        size_t d2 = 0;
        size_t d3 = 0;
        d0 = vec.size();

        if (d0 > 0) {
            d1 = vec.at(0).size();
            if (d1 > 0) {
                d2 = vec.at(0).at(0).size();
                if (d2 > 0) {
                    d3 = vec.at(0).at(0).at(0).size();


                }
            }
        }

        init(d0, d1, d2, d3, array_name);
        for (int i0 = 0; i0 < dim[0]; i0++) {
            if (vec.at(i0).size() != d1)
                throw std::invalid_argument("Vector size is not constant at dimension 1");

            for (int i1 = 0; i1 < dim[1]; i1++) {
                if (vec.at(i0).at(i1).size() != d2)
                    throw std::invalid_argument("Vector size is not constant at dimension 2");

                for (int i2 = 0; i2 < dim[2]; i2++) {
                    if (vec.at(i0).at(i1).at(i2).size() != d3)
                        throw std::invalid_argument("Vector size is not constant at dimension 3");

                    for (int i3 = 0; i3 < dim[3]; i3++) {
                        operator()(i0, i1, i2, i3) = vec.at(i0).at(i1).at(i2).at(i3);

                    }
                }
            }
        }

    }

};

template<class T>
class Array5D : public ContiguousArrayND<T> {
    using ContiguousArrayND<T>::array_name;
    using ContiguousArrayND<T>::data;
    using ContiguousArrayND<T>::size;

    // dimensions
    size_t dim[5] = {0};

    // strides
    size_t s[5] = {0};

    int ndim = 5;

public:

    // default empty constructor
    Array5D() = default;

    // parametrized constructor
    Array5D(const string &array_name) { this->array_name = array_name; }

    // parametrized constructor
    Array5D(size_t d0, size_t d1, size_t d2, size_t d3, size_t d4, const string &array_name = "Array5D") {
        init(d0, d1, d2, d3, d4, array_name);
    }

    //initialize array and strides
    void init(size_t d0, size_t d1, size_t d2, size_t d3, size_t d4, const string &array_name = "Array5D") {
        this->array_name = array_name;


        dim[0] = d0;
        dim[1] = d1;
        dim[2] = d2;
        dim[3] = d3;
        dim[4] = d4;

        s[4] = 1;
        s[3] = s[4] * dim[4];
        s[2] = s[3] * dim[3];
        s[1] = s[2] * dim[2];
        s[0] = s[1] * dim[1];

        if (size != s[0] * dim[0]) {
            size = s[0] * dim[0];
            if (data) delete[] data;
            data = new T[size];
            memset(data, 0, size * sizeof(T));
        } else {
            memset(data, 0, size * sizeof(T));
        }
    }

    void resize(size_t d0, size_t d1, size_t d2, size_t d3, size_t d4) {
        init(d0, d1, d2, d3, d4, this->array_name);
    }

    size_t get_dim(int d) const {
        return dim[d];
    }

#ifdef MULTIARRAY_INDICES_CHECK
    void check_indices(size_t i0, size_t i1, size_t i2, size_t i3, size_t i4) const {

        if((i0<0)|(i0>=dim[0])){
            printf("%s: index i0=%ld out of range (0, %ld)\n", array_name.c_str(), i0, dim[0]-1);
            exit(EXIT_FAILURE);
        }

        if((i1<0)|(i1>=dim[1])){
            printf("%s: index i1=%ld out of range (0, %ld)\n", array_name.c_str(), i1, dim[1]-1);
            exit(EXIT_FAILURE);
        }

        if((i2<0)|(i2>=dim[2])){
            printf("%s: index i2=%ld out of range (0, %ld)\n", array_name.c_str(), i2, dim[2]-1);
            exit(EXIT_FAILURE);
        }

        if((i3<0)|(i3>=dim[3])){
            printf("%s: index i3=%ld out of range (0, %ld)\n", array_name.c_str(), i3, dim[3]-1);
            exit(EXIT_FAILURE);
        }

        if((i4<0)|(i4>=dim[4])){
            printf("%s: index i4=%ld out of range (0, %ld)\n", array_name.c_str(), i4, dim[4]-1);
            exit(EXIT_FAILURE);
        }

    }
#endif

    inline const T &operator()(size_t i0, size_t i1, size_t i2, size_t i3, size_t i4) const {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1, i2, i3, i4);
#endif

        return data[i0 * s[0] + i1 * s[1] + i2 * s[2] + i3 * s[3] + i4];

    }

    inline T &operator()(size_t i0, size_t i1, size_t i2, size_t i3, size_t i4) {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1, i2, i3, i4);
#endif

        return data[i0 * s[0] + i1 * s[1] + i2 * s[2] + i3 * s[3] + i4];

    }

    bool operator==(const Array5D &other) const {
        //compare dimensions
        for (int d = 0; d < ndim; d++) {
            if (this->dim[d] != other.dim[d])
                return false;
        }
        return ContiguousArrayND<T>::operator==(other);
    }

    vector<vector<vector<vector<vector<T>>>>> to_vector() const {
        vector<vector<vector<vector<vector<T>>>>> res;

        res.resize(dim[0]);

        for (int i0 = 0; i0 < dim[0]; i0++) {
            res[i0].resize(dim[1]);


            for (int i1 = 0; i1 < dim[1]; i1++) {
                res[i0][i1].resize(dim[2]);


                for (int i2 = 0; i2 < dim[2]; i2++) {
                    res[i0][i1][i2].resize(dim[3]);


                    for (int i3 = 0; i3 < dim[3]; i3++) {
                        res[i0][i1][i2][i3].resize(dim[4]);


                        for (int i4 = 0; i4 < dim[4]; i4++) {
                            res[i0][i1][i2][i3][i4] = operator()(i0, i1, i2, i3, i4);

                        }
                    }
                }
            }
        }


        return res;
    } // end to_vector()

    // parametrized constructor from vector
    Array5D(vector<vector<vector<vector<vector<T>>>>> vec, const string &array_name = "Array5D") {
        size_t d0 = 0;
        size_t d1 = 0;
        size_t d2 = 0;
        size_t d3 = 0;
        size_t d4 = 0;
        d0 = vec.size();

        if (d0 > 0) {
            d1 = vec.at(0).size();
            if (d1 > 0) {
                d2 = vec.at(0).at(0).size();
                if (d2 > 0) {
                    d3 = vec.at(0).at(0).at(0).size();
                    if (d3 > 0) {
                        d4 = vec.at(0).at(0).at(0).at(0).size();


                    }
                }
            }
        }

        init(d0, d1, d2, d3, d4, array_name);
        for (int i0 = 0; i0 < dim[0]; i0++) {
            if (vec.at(i0).size() != d1)
                throw std::invalid_argument("Vector size is not constant at dimension 1");

            for (int i1 = 0; i1 < dim[1]; i1++) {
                if (vec.at(i0).at(i1).size() != d2)
                    throw std::invalid_argument("Vector size is not constant at dimension 2");

                for (int i2 = 0; i2 < dim[2]; i2++) {
                    if (vec.at(i0).at(i1).at(i2).size() != d3)
                        throw std::invalid_argument("Vector size is not constant at dimension 3");

                    for (int i3 = 0; i3 < dim[3]; i3++) {
                        if (vec.at(i0).at(i1).at(i2).at(i3).size() != d4)
                            throw std::invalid_argument("Vector size is not constant at dimension 4");

                        for (int i4 = 0; i4 < dim[4]; i4++) {
                            operator()(i0, i1, i2, i3, i4) = vec.at(i0).at(i1).at(i2).at(i3).at(i4);

                        }
                    }
                }
            }
        }

    }

};

template<class T>
class Array6D : public ContiguousArrayND<T> {
    using ContiguousArrayND<T>::array_name;
    using ContiguousArrayND<T>::data;
    using ContiguousArrayND<T>::size;

    // dimensions
    size_t dim[6] = {0};

    // strides
    size_t s[6] = {0};

    int ndim = 6;

public:

    // default empty constructor
    Array6D() = default;

    // parametrized constructor
    Array6D(const string &array_name) { this->array_name = array_name; }

    // parametrized constructor
    Array6D(size_t d0, size_t d1, size_t d2, size_t d3, size_t d4, size_t d5, const string &array_name = "Array6D") {
        init(d0, d1, d2, d3, d4, d5, array_name);
    }

    //initialize array and strides
    void init(size_t d0, size_t d1, size_t d2, size_t d3, size_t d4, size_t d5, const string &array_name = "Array6D") {
        this->array_name = array_name;


        dim[0] = d0;
        dim[1] = d1;
        dim[2] = d2;
        dim[3] = d3;
        dim[4] = d4;
        dim[5] = d5;

        s[5] = 1;
        s[4] = s[5] * dim[5];
        s[3] = s[4] * dim[4];
        s[2] = s[3] * dim[3];
        s[1] = s[2] * dim[2];
        s[0] = s[1] * dim[1];

        if (size != s[0] * dim[0]) {
            size = s[0] * dim[0];
            if (data) delete[] data;
            data = new T[size];
            memset(data, 0, size * sizeof(T));
        } else {
            memset(data, 0, size * sizeof(T));
        }
    }

    void resize(size_t d0, size_t d1, size_t d2, size_t d3, size_t d4, size_t d5) {
        init(d0, d1, d2, d3, d4, d5, this->array_name);
    }

    size_t get_dim(int d) const {
        return dim[d];
    }

#ifdef MULTIARRAY_INDICES_CHECK
    void check_indices(size_t i0, size_t i1, size_t i2, size_t i3, size_t i4, size_t i5) const {

        if((i0<0)|(i0>=dim[0])){
            printf("%s: index i0=%ld out of range (0, %ld)\n", array_name.c_str(), i0, dim[0]-1);
            exit(EXIT_FAILURE);
        }

        if((i1<0)|(i1>=dim[1])){
            printf("%s: index i1=%ld out of range (0, %ld)\n", array_name.c_str(), i1, dim[1]-1);
            exit(EXIT_FAILURE);
        }

        if((i2<0)|(i2>=dim[2])){
            printf("%s: index i2=%ld out of range (0, %ld)\n", array_name.c_str(), i2, dim[2]-1);
            exit(EXIT_FAILURE);
        }

        if((i3<0)|(i3>=dim[3])){
            printf("%s: index i3=%ld out of range (0, %ld)\n", array_name.c_str(), i3, dim[3]-1);
            exit(EXIT_FAILURE);
        }

        if((i4<0)|(i4>=dim[4])){
            printf("%s: index i4=%ld out of range (0, %ld)\n", array_name.c_str(), i4, dim[4]-1);
            exit(EXIT_FAILURE);
        }

        if((i5<0)|(i5>=dim[5])){
            printf("%s: index i5=%ld out of range (0, %ld)\n", array_name.c_str(), i5, dim[5]-1);
            exit(EXIT_FAILURE);
        }

    }
#endif

    inline const T &operator()(size_t i0, size_t i1, size_t i2, size_t i3, size_t i4, size_t i5) const {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1, i2, i3, i4, i5);
#endif

        return data[i0 * s[0] + i1 * s[1] + i2 * s[2] + i3 * s[3] + i4 * s[4] + i5];

    }

    inline T &operator()(size_t i0, size_t i1, size_t i2, size_t i3, size_t i4, size_t i5) {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1, i2, i3, i4, i5);
#endif

        return data[i0 * s[0] + i1 * s[1] + i2 * s[2] + i3 * s[3] + i4 * s[4] + i5];

    }

    bool operator==(const Array6D &other) const {
        //compare dimensions
        for (int d = 0; d < ndim; d++) {
            if (this->dim[d] != other.dim[d])
                return false;
        }
        return ContiguousArrayND<T>::operator==(other);
    }

    vector<vector<vector<vector<vector<vector<T>>>>>> to_vector() const {
        vector<vector<vector<vector<vector<vector<T>>>>>> res;

        res.resize(dim[0]);

        for (int i0 = 0; i0 < dim[0]; i0++) {
            res[i0].resize(dim[1]);


            for (int i1 = 0; i1 < dim[1]; i1++) {
                res[i0][i1].resize(dim[2]);


                for (int i2 = 0; i2 < dim[2]; i2++) {
                    res[i0][i1][i2].resize(dim[3]);


                    for (int i3 = 0; i3 < dim[3]; i3++) {
                        res[i0][i1][i2][i3].resize(dim[4]);


                        for (int i4 = 0; i4 < dim[4]; i4++) {
                            res[i0][i1][i2][i3][i4].resize(dim[5]);


                            for (int i5 = 0; i5 < dim[5]; i5++) {
                                res[i0][i1][i2][i3][i4][i5] = operator()(i0, i1, i2, i3, i4, i5);

                            }
                        }
                    }
                }
            }
        }


        return res;
    } // end to_vector()

    // parametrized constructor from vector
    Array6D(vector<vector<vector<vector<vector<vector<T>>>>>> vec, const string &array_name = "Array6D") {
        size_t d0 = 0;
        size_t d1 = 0;
        size_t d2 = 0;
        size_t d3 = 0;
        size_t d4 = 0;
        size_t d5 = 0;
        d0 = vec.size();

        if (d0 > 0) {
            d1 = vec.at(0).size();
            if (d1 > 0) {
                d2 = vec.at(0).at(0).size();
                if (d2 > 0) {
                    d3 = vec.at(0).at(0).at(0).size();
                    if (d3 > 0) {
                        d4 = vec.at(0).at(0).at(0).at(0).size();
                        if (d4 > 0) {
                            d5 = vec.at(0).at(0).at(0).at(0).at(0).size();


                        }
                    }
                }
            }
        }

        init(d0, d1, d2, d3, d4, d5, array_name);
        for (int i0 = 0; i0 < dim[0]; i0++) {
            if (vec.at(i0).size() != d1)
                throw std::invalid_argument("Vector size is not constant at dimension 1");

            for (int i1 = 0; i1 < dim[1]; i1++) {
                if (vec.at(i0).at(i1).size() != d2)
                    throw std::invalid_argument("Vector size is not constant at dimension 2");

                for (int i2 = 0; i2 < dim[2]; i2++) {
                    if (vec.at(i0).at(i1).at(i2).size() != d3)
                        throw std::invalid_argument("Vector size is not constant at dimension 3");

                    for (int i3 = 0; i3 < dim[3]; i3++) {
                        if (vec.at(i0).at(i1).at(i2).at(i3).size() != d4)
                            throw std::invalid_argument("Vector size is not constant at dimension 4");

                        for (int i4 = 0; i4 < dim[4]; i4++) {
                            if (vec.at(i0).at(i1).at(i2).at(i3).at(i4).size() != d5)
                                throw std::invalid_argument("Vector size is not constant at dimension 5");

                            for (int i5 = 0; i5 < dim[5]; i5++) {
                                operator()(i0, i1, i2, i3, i4, i5) = vec.at(i0).at(i1).at(i2).at(i3).at(i4).at(i5);

                            }
                        }
                    }
                }
            }
        }

    }

};

template<class T>
class Array1DGeneral : public ContiguousArrayND<T> {
    using ContiguousArrayND<T>::array_name;
    using ContiguousArrayND<T>::data;
    using ContiguousArrayND<T>::size;

    // ranges
    int i0_start, i0_stop;

    // dimensions
    size_t dim[1] = {0};

    // strides
    size_t s[1] = {0};

    int ndim = 1;

public:
    // default empty constructor
    Array1DGeneral() {}

    // parametrized constructor
    Array1DGeneral(const string &array_name) { this->array_name = array_name; }

    // parametrized constructor
    Array1DGeneral(int i0_init, int i0_final, const string &array_name = "Array1DGeneral") {
        init(i0_init, i0_final, array_name);
    }

    //initialize array and strides
    void init(int i0_init, int i0_final, const string &array_name = "Array1DGeneral") {
        this->array_name = array_name;

        i0_start = i0_init;
        i0_stop = i0_final;


        dim[0] = i0_final - i0_init + 1;

        s[0] = 1;

        if (size != s[0] * dim[0]) {
            size = s[0] * dim[0];
            if (data) delete[] data;
            data = new T[size];
            memset(data, 0, size * sizeof(T));
        } else {
            memset(data, 0, size * sizeof(T));
        }
    }

    void resize(int i0_init, int i0_final) {
        init(i0_init, i0_final, this->array_name);
    }

    size_t get_dim(int d) const {
        return dim[d];
    }

#ifdef MULTIARRAY_INDICES_CHECK
    void check_indices(int i0) const {

        if(i0<i0_start|i0>i0_stop){
            printf("%s: index i0=%ld out of range (%ld, %ld)\n", array_name.c_str(), i0, i0_start, i0_stop);
            exit(EXIT_FAILURE);
        }

    }
#endif

    inline const T &operator()(int i0) const {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0);
#endif
        return data[(i0 - i0_start)];
    }

    inline T &operator()(int i0) {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0);
#endif
        return data[(i0 - i0_start)];
    }

    bool operator==(const Array1DGeneral &other) const {
        //compare dimensions
        for (int d = 0; d < ndim; d++) {
            if (this->dim[d] != other.dim[d])
                return false;
        }
        return ContiguousArrayND<T>::operator==(other);
    }
};

template<class T>
class Array2DGeneral : public ContiguousArrayND<T> {
    using ContiguousArrayND<T>::array_name;
    using ContiguousArrayND<T>::data;
    using ContiguousArrayND<T>::size;

    // ranges
    int i0_start, i0_stop, i1_start, i1_stop;

    // dimensions
    size_t dim[2] = {0};

    // strides
    size_t s[2] = {0};

    int ndim = 2;

public:
    // default empty constructor
    Array2DGeneral() {}

    // parametrized constructor
    Array2DGeneral(const string &array_name) { this->array_name = array_name; }

    // parametrized constructor
    Array2DGeneral(int i0_init, int i0_final, int i1_init, int i1_final, const string &array_name = "Array2DGeneral") {
        init(i0_init, i0_final, i1_init, i1_final, array_name);
    }

    //initialize array and strides
    void init(int i0_init, int i0_final, int i1_init, int i1_final, const string &array_name = "Array2DGeneral") {
        this->array_name = array_name;

        i0_start = i0_init;
        i0_stop = i0_final;
        i1_start = i1_init;
        i1_stop = i1_final;


        dim[0] = i0_final - i0_init + 1;
        dim[1] = i1_final - i1_init + 1;

        s[1] = 1;
        s[0] = s[1] * dim[1];

        if (size != s[0] * dim[0]) {
            size = s[0] * dim[0];
            if (data) delete[] data;
            data = new T[size];
            memset(data, 0, size * sizeof(T));
        } else {
            memset(data, 0, size * sizeof(T));
        }
    }

    void resize(int i0_init, int i0_final, int i1_init, int i1_final) {
        init(i0_init, i0_final, i1_init, i1_final, this->array_name);
    }

    size_t get_dim(int d) const {
        return dim[d];
    }

#ifdef MULTIARRAY_INDICES_CHECK
    void check_indices(int i0, int i1) const {

        if(i0<i0_start|i0>i0_stop){
            printf("%s: index i0=%ld out of range (%ld, %ld)\n", array_name.c_str(), i0, i0_start, i0_stop);
            exit(EXIT_FAILURE);
        }

        if(i1<i1_start|i1>i1_stop){
            printf("%s: index i1=%ld out of range (%ld, %ld)\n", array_name.c_str(), i1, i1_start, i1_stop);
            exit(EXIT_FAILURE);
        }

    }
#endif

    inline const T &operator()(int i0, int i1) const {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1);
#endif
        return data[(i0 - i0_start) * s[0] + (i1 - i1_start)];
    }

    inline T &operator()(int i0, int i1) {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1);
#endif
        return data[(i0 - i0_start) * s[0] + (i1 - i1_start)];
    }

    bool operator==(const Array2DGeneral &other) const {
        //compare dimensions
        for (int d = 0; d < ndim; d++) {
            if (this->dim[d] != other.dim[d])
                return false;
        }
        return ContiguousArrayND<T>::operator==(other);
    }
};

template<class T>
class Array3DGeneral : public ContiguousArrayND<T> {
    using ContiguousArrayND<T>::array_name;
    using ContiguousArrayND<T>::data;
    using ContiguousArrayND<T>::size;

    // ranges
    int i0_start, i0_stop, i1_start, i1_stop, i2_start, i2_stop;

    // dimensions
    size_t dim[3] = {0};

    // strides
    size_t s[3] = {0};

    int ndim = 3;

public:
    // default empty constructor
    Array3DGeneral() {}

    // parametrized constructor
    Array3DGeneral(const string &array_name) { this->array_name = array_name; }

    // parametrized constructor
    Array3DGeneral(int i0_init, int i0_final, int i1_init, int i1_final, int i2_init, int i2_final,
                   const string &array_name = "Array3DGeneral") {
        init(i0_init, i0_final, i1_init, i1_final, i2_init, i2_final, array_name);
    }

    //initialize array and strides
    void init(int i0_init, int i0_final, int i1_init, int i1_final, int i2_init, int i2_final,
              const string &array_name = "Array3DGeneral") {
        this->array_name = array_name;

        i0_start = i0_init;
        i0_stop = i0_final;
        i1_start = i1_init;
        i1_stop = i1_final;
        i2_start = i2_init;
        i2_stop = i2_final;


        dim[0] = i0_final - i0_init + 1;
        dim[1] = i1_final - i1_init + 1;
        dim[2] = i2_final - i2_init + 1;

        s[2] = 1;
        s[1] = s[2] * dim[2];
        s[0] = s[1] * dim[1];

        if (size != s[0] * dim[0]) {
            size = s[0] * dim[0];
            if (data) delete[] data;
            data = new T[size];
            memset(data, 0, size * sizeof(T));
        } else {
            memset(data, 0, size * sizeof(T));
        }
    }

    void resize(int i0_init, int i0_final, int i1_init, int i1_final, int i2_init, int i2_final) {
        init(i0_init, i0_final, i1_init, i1_final, i2_init, i2_final, this->array_name);
    }

    size_t get_dim(int d) const {
        return dim[d];
    }

#ifdef MULTIARRAY_INDICES_CHECK
    void check_indices(int i0, int i1, int i2) const {

        if(i0<i0_start|i0>i0_stop){
            printf("%s: index i0=%ld out of range (%ld, %ld)\n", array_name.c_str(), i0, i0_start, i0_stop);
            exit(EXIT_FAILURE);
        }

        if(i1<i1_start|i1>i1_stop){
            printf("%s: index i1=%ld out of range (%ld, %ld)\n", array_name.c_str(), i1, i1_start, i1_stop);
            exit(EXIT_FAILURE);
        }

        if(i2<i2_start|i2>i2_stop){
            printf("%s: index i2=%ld out of range (%ld, %ld)\n", array_name.c_str(), i2, i2_start, i2_stop);
            exit(EXIT_FAILURE);
        }

    }
#endif

    inline const T &operator()(int i0, int i1, int i2) const {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1, i2);
#endif
        return data[(i0 - i0_start) * s[0] + (i1 - i1_start) * s[1] + (i2 - i2_start)];
    }

    inline T &operator()(int i0, int i1, int i2) {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1, i2);
#endif
        return data[(i0 - i0_start) * s[0] + (i1 - i1_start) * s[1] + (i2 - i2_start)];
    }

    bool operator==(const Array3DGeneral &other) const {
        //compare dimensions
        for (int d = 0; d < ndim; d++) {
            if (this->dim[d] != other.dim[d])
                return false;
        }
        return ContiguousArrayND<T>::operator==(other);
    }
};

template<class T>
class Array4DGeneral : public ContiguousArrayND<T> {
    using ContiguousArrayND<T>::array_name;
    using ContiguousArrayND<T>::data;
    using ContiguousArrayND<T>::size;

    // ranges
    int i0_start, i0_stop, i1_start, i1_stop, i2_start, i2_stop, i3_start, i3_stop;

    // dimensions
    size_t dim[4] = {0};

    // strides
    size_t s[4] = {0};

    int ndim = 4;

public:
    // default empty constructor
    Array4DGeneral() {}

    // parametrized constructor
    Array4DGeneral(const string &array_name) { this->array_name = array_name; }

    // parametrized constructor
    Array4DGeneral(int i0_init, int i0_final, int i1_init, int i1_final, int i2_init, int i2_final, int i3_init,
                   int i3_final, const string &array_name = "Array4DGeneral") {
        init(i0_init, i0_final, i1_init, i1_final, i2_init, i2_final, i3_init, i3_final, array_name);
    }

    //initialize array and strides
    void
    init(int i0_init, int i0_final, int i1_init, int i1_final, int i2_init, int i2_final, int i3_init, int i3_final,
         const string &array_name = "Array4DGeneral") {
        this->array_name = array_name;

        i0_start = i0_init;
        i0_stop = i0_final;
        i1_start = i1_init;
        i1_stop = i1_final;
        i2_start = i2_init;
        i2_stop = i2_final;
        i3_start = i3_init;
        i3_stop = i3_final;


        dim[0] = i0_final - i0_init + 1;
        dim[1] = i1_final - i1_init + 1;
        dim[2] = i2_final - i2_init + 1;
        dim[3] = i3_final - i3_init + 1;

        s[3] = 1;
        s[2] = s[3] * dim[3];
        s[1] = s[2] * dim[2];
        s[0] = s[1] * dim[1];

        if (size != s[0] * dim[0]) {
            size = s[0] * dim[0];
            if (data) delete[] data;
            data = new T[size];
            memset(data, 0, size * sizeof(T));
        } else {
            memset(data, 0, size * sizeof(T));
        }
    }

    void
    resize(int i0_init, int i0_final, int i1_init, int i1_final, int i2_init, int i2_final, int i3_init, int i3_final) {
        init(i0_init, i0_final, i1_init, i1_final, i2_init, i2_final, i3_init, i3_final, this->array_name);
    }

    size_t get_dim(int d) const {
        return dim[d];
    }

#ifdef MULTIARRAY_INDICES_CHECK
    void check_indices(int i0, int i1, int i2, int i3) const {

        if(i0<i0_start|i0>i0_stop){
            printf("%s: index i0=%ld out of range (%ld, %ld)\n", array_name.c_str(), i0, i0_start, i0_stop);
            exit(EXIT_FAILURE);
        }

        if(i1<i1_start|i1>i1_stop){
            printf("%s: index i1=%ld out of range (%ld, %ld)\n", array_name.c_str(), i1, i1_start, i1_stop);
            exit(EXIT_FAILURE);
        }

        if(i2<i2_start|i2>i2_stop){
            printf("%s: index i2=%ld out of range (%ld, %ld)\n", array_name.c_str(), i2, i2_start, i2_stop);
            exit(EXIT_FAILURE);
        }

        if(i3<i3_start|i3>i3_stop){
            printf("%s: index i3=%ld out of range (%ld, %ld)\n", array_name.c_str(), i3, i3_start, i3_stop);
            exit(EXIT_FAILURE);
        }

    }
#endif

    inline const T &operator()(int i0, int i1, int i2, int i3) const {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1, i2, i3);
#endif
        return data[(i0 - i0_start) * s[0] + (i1 - i1_start) * s[1] + (i2 - i2_start) * s[2] + (i3 - i3_start)];
    }

    inline T &operator()(int i0, int i1, int i2, int i3) {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1, i2, i3);
#endif
        return data[(i0 - i0_start) * s[0] + (i1 - i1_start) * s[1] + (i2 - i2_start) * s[2] + (i3 - i3_start)];
    }

    bool operator==(const Array4DGeneral &other) const {
        //compare dimensions
        for (int d = 0; d < ndim; d++) {
            if (this->dim[d] != other.dim[d])
                return false;
        }
        return ContiguousArrayND<T>::operator==(other);
    }
};

template<class T>
class Array5DGeneral : public ContiguousArrayND<T> {
    using ContiguousArrayND<T>::array_name;
    using ContiguousArrayND<T>::data;
    using ContiguousArrayND<T>::size;

    // ranges
    int i0_start, i0_stop, i1_start, i1_stop, i2_start, i2_stop, i3_start, i3_stop, i4_start, i4_stop;

    // dimensions
    size_t dim[5] = {0};

    // strides
    size_t s[5] = {0};

    int ndim = 5;

public:
    // default empty constructor
    Array5DGeneral() {}

    // parametrized constructor
    Array5DGeneral(const string &array_name) { this->array_name = array_name; }

    // parametrized constructor
    Array5DGeneral(int i0_init, int i0_final, int i1_init, int i1_final, int i2_init, int i2_final, int i3_init,
                   int i3_final, int i4_init, int i4_final, const string &array_name = "Array5DGeneral") {
        init(i0_init, i0_final, i1_init, i1_final, i2_init, i2_final, i3_init, i3_final, i4_init, i4_final, array_name);
    }

    //initialize array and strides
    void
    init(int i0_init, int i0_final, int i1_init, int i1_final, int i2_init, int i2_final, int i3_init, int i3_final,
         int i4_init, int i4_final, const string &array_name = "Array5DGeneral") {
        this->array_name = array_name;

        i0_start = i0_init;
        i0_stop = i0_final;
        i1_start = i1_init;
        i1_stop = i1_final;
        i2_start = i2_init;
        i2_stop = i2_final;
        i3_start = i3_init;
        i3_stop = i3_final;
        i4_start = i4_init;
        i4_stop = i4_final;


        dim[0] = i0_final - i0_init + 1;
        dim[1] = i1_final - i1_init + 1;
        dim[2] = i2_final - i2_init + 1;
        dim[3] = i3_final - i3_init + 1;
        dim[4] = i4_final - i4_init + 1;

        s[4] = 1;
        s[3] = s[4] * dim[4];
        s[2] = s[3] * dim[3];
        s[1] = s[2] * dim[2];
        s[0] = s[1] * dim[1];

        if (size != s[0] * dim[0]) {
            size = s[0] * dim[0];
            if (data) delete[] data;
            data = new T[size];
            memset(data, 0, size * sizeof(T));
        } else {
            memset(data, 0, size * sizeof(T));
        }
    }

    void
    resize(int i0_init, int i0_final, int i1_init, int i1_final, int i2_init, int i2_final, int i3_init, int i3_final,
           int i4_init, int i4_final) {
        init(i0_init, i0_final, i1_init, i1_final, i2_init, i2_final, i3_init, i3_final, i4_init, i4_final,
             this->array_name);
    }

    size_t get_dim(int d) const {
        return dim[d];
    }

#ifdef MULTIARRAY_INDICES_CHECK
    void check_indices(int i0, int i1, int i2, int i3, int i4) const {

        if(i0<i0_start|i0>i0_stop){
            printf("%s: index i0=%ld out of range (%ld, %ld)\n", array_name.c_str(), i0, i0_start, i0_stop);
            exit(EXIT_FAILURE);
        }

        if(i1<i1_start|i1>i1_stop){
            printf("%s: index i1=%ld out of range (%ld, %ld)\n", array_name.c_str(), i1, i1_start, i1_stop);
            exit(EXIT_FAILURE);
        }

        if(i2<i2_start|i2>i2_stop){
            printf("%s: index i2=%ld out of range (%ld, %ld)\n", array_name.c_str(), i2, i2_start, i2_stop);
            exit(EXIT_FAILURE);
        }

        if(i3<i3_start|i3>i3_stop){
            printf("%s: index i3=%ld out of range (%ld, %ld)\n", array_name.c_str(), i3, i3_start, i3_stop);
            exit(EXIT_FAILURE);
        }

        if(i4<i4_start|i4>i4_stop){
            printf("%s: index i4=%ld out of range (%ld, %ld)\n", array_name.c_str(), i4, i4_start, i4_stop);
            exit(EXIT_FAILURE);
        }

    }
#endif

    inline const T &operator()(int i0, int i1, int i2, int i3, int i4) const {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1, i2, i3, i4);
#endif
        return data[(i0 - i0_start) * s[0] + (i1 - i1_start) * s[1] + (i2 - i2_start) * s[2] + (i3 - i3_start) * s[3] +
                    (i4 - i4_start)];
    }

    inline T &operator()(int i0, int i1, int i2, int i3, int i4) {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1, i2, i3, i4);
#endif
        return data[(i0 - i0_start) * s[0] + (i1 - i1_start) * s[1] + (i2 - i2_start) * s[2] + (i3 - i3_start) * s[3] +
                    (i4 - i4_start)];
    }

    bool operator==(const Array5DGeneral &other) const {
        //compare dimensions
        for (int d = 0; d < ndim; d++) {
            if (this->dim[d] != other.dim[d])
                return false;
        }
        return ContiguousArrayND<T>::operator==(other);
    }
};

template<class T>
class Array6DGeneral : public ContiguousArrayND<T> {
    using ContiguousArrayND<T>::array_name;
    using ContiguousArrayND<T>::data;
    using ContiguousArrayND<T>::size;

    // ranges
    int i0_start, i0_stop, i1_start, i1_stop, i2_start, i2_stop, i3_start, i3_stop, i4_start, i4_stop, i5_start, i5_stop;

    // dimensions
    size_t dim[6] = {0};

    // strides
    size_t s[6] = {0};

    int ndim = 6;

public:
    // default empty constructor
    Array6DGeneral() {}

    // parametrized constructor
    Array6DGeneral(const string &array_name) { this->array_name = array_name; }

    // parametrized constructor
    Array6DGeneral(int i0_init, int i0_final, int i1_init, int i1_final, int i2_init, int i2_final, int i3_init,
                   int i3_final, int i4_init, int i4_final, int i5_init, int i5_final,
                   const string &array_name = "Array6DGeneral") {
        init(i0_init, i0_final, i1_init, i1_final, i2_init, i2_final, i3_init, i3_final, i4_init, i4_final, i5_init,
             i5_final, array_name);
    }

    //initialize array and strides
    void
    init(int i0_init, int i0_final, int i1_init, int i1_final, int i2_init, int i2_final, int i3_init, int i3_final,
         int i4_init, int i4_final, int i5_init, int i5_final, const string &array_name = "Array6DGeneral") {
        this->array_name = array_name;

        i0_start = i0_init;
        i0_stop = i0_final;
        i1_start = i1_init;
        i1_stop = i1_final;
        i2_start = i2_init;
        i2_stop = i2_final;
        i3_start = i3_init;
        i3_stop = i3_final;
        i4_start = i4_init;
        i4_stop = i4_final;
        i5_start = i5_init;
        i5_stop = i5_final;


        dim[0] = i0_final - i0_init + 1;
        dim[1] = i1_final - i1_init + 1;
        dim[2] = i2_final - i2_init + 1;
        dim[3] = i3_final - i3_init + 1;
        dim[4] = i4_final - i4_init + 1;
        dim[5] = i5_final - i5_init + 1;

        s[5] = 1;
        s[4] = s[5] * dim[5];
        s[3] = s[4] * dim[4];
        s[2] = s[3] * dim[3];
        s[1] = s[2] * dim[2];
        s[0] = s[1] * dim[1];

        if (size != s[0] * dim[0]) {
            size = s[0] * dim[0];
            if (data) delete[] data;
            data = new T[size];
            memset(data, 0, size * sizeof(T));
        } else {
            memset(data, 0, size * sizeof(T));
        }
    }

    void
    resize(int i0_init, int i0_final, int i1_init, int i1_final, int i2_init, int i2_final, int i3_init, int i3_final,
           int i4_init, int i4_final, int i5_init, int i5_final) {
        init(i0_init, i0_final, i1_init, i1_final, i2_init, i2_final, i3_init, i3_final, i4_init, i4_final, i5_init,
             i5_final, this->array_name);
    }

    size_t get_dim(int d) const {
        return dim[d];
    }

#ifdef MULTIARRAY_INDICES_CHECK
    void check_indices(int i0, int i1, int i2, int i3, int i4, int i5) const {

        if(i0<i0_start|i0>i0_stop){
            printf("%s: index i0=%ld out of range (%ld, %ld)\n", array_name.c_str(), i0, i0_start, i0_stop);
            exit(EXIT_FAILURE);
        }

        if(i1<i1_start|i1>i1_stop){
            printf("%s: index i1=%ld out of range (%ld, %ld)\n", array_name.c_str(), i1, i1_start, i1_stop);
            exit(EXIT_FAILURE);
        }

        if(i2<i2_start|i2>i2_stop){
            printf("%s: index i2=%ld out of range (%ld, %ld)\n", array_name.c_str(), i2, i2_start, i2_stop);
            exit(EXIT_FAILURE);
        }

        if(i3<i3_start|i3>i3_stop){
            printf("%s: index i3=%ld out of range (%ld, %ld)\n", array_name.c_str(), i3, i3_start, i3_stop);
            exit(EXIT_FAILURE);
        }

        if(i4<i4_start|i4>i4_stop){
            printf("%s: index i4=%ld out of range (%ld, %ld)\n", array_name.c_str(), i4, i4_start, i4_stop);
            exit(EXIT_FAILURE);
        }

        if(i5<i5_start|i5>i5_stop){
            printf("%s: index i5=%ld out of range (%ld, %ld)\n", array_name.c_str(), i5, i5_start, i5_stop);
            exit(EXIT_FAILURE);
        }

    }
#endif

    inline const T &operator()(int i0, int i1, int i2, int i3, int i4, int i5) const {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1, i2, i3, i4, i5);
#endif
        return data[(i0 - i0_start) * s[0] + (i1 - i1_start) * s[1] + (i2 - i2_start) * s[2] + (i3 - i3_start) * s[3] +
                    (i4 - i4_start) * s[4] + (i5 - i5_start)];
    }

    inline T &operator()(int i0, int i1, int i2, int i3, int i4, int i5) {
#ifdef MULTIARRAY_INDICES_CHECK
        check_indices(i0, i1, i2, i3, i4, i5);
#endif
        return data[(i0 - i0_start) * s[0] + (i1 - i1_start) * s[1] + (i2 - i2_start) * s[2] + (i3 - i3_start) * s[3] +
                    (i4 - i4_start) * s[4] + (i5 - i5_start)];
    }

    bool operator==(const Array6DGeneral &other) const {
        //compare dimensions
        for (int d = 0; d < ndim; d++) {
            if (this->dim[d] != other.dim[d])
                return false;
        }
        return ContiguousArrayND<T>::operator==(other);
    }
};


#endif //ACE_MULTIARRAY_H
