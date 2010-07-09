#include "dataFile.h"
#include "debugUtils.h"
#include "boost/filesystem.hpp"

using std::cout;
using std::endl;

DataFile::DataFile(void)
    : file(0)
    , fileIsOpen(false)
{
}

DataFile::DataFile(const std::string& in_filename)
    : file(0)
{
    open(in_filename);
}

DataFile::~DataFile(void)
{
    close();
}

void DataFile::writeScalar(const std::string& name, const double x)
{
    require_file_open();

    hid_t dataspace, dataset, datatype;
    dataspace = H5Screate_simple(0, NULL, NULL);
    datatype = H5Tcopy(H5T_NATIVE_DOUBLE);
    H5Tset_order(datatype, H5T_ORDER_LE);

    dataset = get_dataset(name, datatype, dataspace);
    H5Dwrite(dataset, datatype, H5S_ALL, H5S_ALL, H5P_DEFAULT, &x);
    H5Dclose(dataset);
}

double DataFile::readScalar(const std::string& name)
{
    require_file_open();

    hid_t dataspace, dataset;
    dataset = H5Dopen(file, name.c_str(), H5P_DEFAULT);
    dataspace = H5Dget_space(dataset);

    if (H5Sget_simple_extent_ndims(dataspace) !=  0) {
        throw debugException("DataFile::readScalar: \"" + name + "\" is not a scalar.");
    }

    double value;
    H5Dread(dataset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &value);
    H5Dclose(dataset);
    return value;
}

void DataFile::writeVector(const std::string& name, const dvector& v)
{
    require_file_open();

    hsize_t dims = v.size();
    if (dims == 0) {
        cout << "DataFile::writeVector: Warning: '" << name << "' is empty." << endl;
        return;
    }

    hid_t dataspace, dataset, datatype;
    dataspace = H5Screate_simple(1, &dims, NULL);
    datatype = H5Tcopy(H5T_NATIVE_DOUBLE);
    H5Tset_order(datatype, H5T_ORDER_LE);

    dataset = get_dataset(name, datatype, dataspace);
    H5Dwrite(dataset, datatype, H5S_ALL, H5S_ALL, H5P_DEFAULT, &v[0]);
    H5Dclose(dataset);
}

dvector DataFile::readVector(const std::string& name)
{
    require_file_open();

    hid_t dataspace, dataset;
    dataset = H5Dopen(file, name.c_str(), H5P_DEFAULT);
    dataspace = H5Dget_space(dataset);

    if (H5Sget_simple_extent_ndims(dataspace) != 1) {
        throw debugException("DataFile::readVector: \"" + name + "\" is not a 1D array.");
    }

    hsize_t N = H5Sget_simple_extent_npoints(dataspace);
    dvector values(N);

    H5Dread(dataset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &values[0]);
    H5Dclose(dataset);
    return values;
}

void DataFile::writeArray2D(const std::string& name, const Cantera::Array2D& y)
{
    require_file_open();

    // Take the transpose because Array2D is column-major
    Cantera::Array2D yt(y.nColumns(), y.nRows());
    for (size_t i=0; i<y.nRows(); i++) {
        for (size_t j=0; j<y.nColumns(); j++) {
            yt(j,i) = y(i,j);
        }
    }

    hsize_t dims[2];
    dims[0] = yt.nColumns(); // TODO: double check the order of these
    dims[1] = yt.nRows();

    if (dims[0] == 0 || dims[1] == 0) {
        cout << "DataFile::writeArray2D: Warning: '" << name << "' is empty." << endl;
        return;
    }

    hid_t dataspace, dataset, datatype;
    dataspace = H5Screate_simple(2, dims, NULL);
    datatype = H5Tcopy(H5T_NATIVE_DOUBLE);
    H5Tset_order(datatype, H5T_ORDER_LE);

    dataset = get_dataset(name, datatype, dataspace);
    H5Dwrite(dataset, datatype, H5S_ALL, H5S_ALL, H5P_DEFAULT, &yt.data()[0]);
    H5Dclose(dataset);
}

Cantera::Array2D DataFile::readArray2D(const std::string& name)
{
    require_file_open();

    hid_t dataspace, dataset;
    dataset = H5Dopen(file, name.c_str(), H5P_DEFAULT);
    dataspace = H5Dget_space(dataset);

    if (H5Sget_simple_extent_ndims(dataspace) != 2) {
        throw debugException("DataFile::readVector: \"" + name + "\" is not a 1D array.");
    }

    hsize_t ndim = 2;
    vector<hsize_t> dimensions(2);
    H5Sget_simple_extent_dims(dataspace, &dimensions[0], &ndim);

    Cantera::Array2D y(dimensions[0], dimensions[1]);
    Cantera::Array2D yt(dimensions[1], dimensions[0]);

    H5Dread(dataset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &yt.data()[0]);
    H5Dclose(dataset);

    // Take the transpose because Array2D is column-major
    for (size_t i=0; i<yt.nRows(); i++) {
        for (size_t j=0; j<yt.nColumns(); j++) {
            y(j,i) = yt(i,j);
        }
    }

    return y;
}

void DataFile::open(const std::string& in_filename)
{
    if (file) {
        close();
    }

    filename = in_filename;
    if (boost::filesystem::exists(filename)) {
        file = H5Fopen(filename.c_str(), H5F_ACC_RDWR, H5P_DEFAULT);
    } else {
        file = H5Fcreate(filename.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    }
    fileIsOpen = true;
}

void DataFile::close()
{
    if (file) {
        H5Fclose(file);
        file = 0;
    }
    fileIsOpen = false;
}

void DataFile::require_file_open()
{
    if (!file) {
        throw debugException(
            "Can't access DataFile object: H5File object not initialized.");
    } else if (!fileIsOpen) {
        throw debugException(
            "Can't access DataFile object: H5File is closed.");
    }
}

hid_t DataFile::get_dataset(const std::string& name, hid_t datatype, hid_t dataspace)
{
    if (H5Lexists(file, name.c_str(), H5P_DEFAULT)) {
        H5Ldelete(file, name.c_str(), H5P_DEFAULT);
    }
    return H5Dcreate(file, name.c_str(), datatype, dataspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
}

void dataFileTest()
{
    // Test reading from / writing to an HDF5 data file
    DataFile f("test.h5");
    double x = 9001;
    f.writeScalar("x", x);

    dvector v(10);
    for (size_t i=0; i<v.size(); i++) {
        v[i] = i*i;
    }

    f.writeVector("v", v);

    Array2D A(3,7);
    for (size_t i=0; i<A.nRows(); i++) {
        for (size_t j=0; j<A.nColumns(); j++) {
            A(i,j) = 10*i + j;
        }
    }

    cout << "rows:" << A.nRows() << endl;
    cout << "cols:" << A.nColumns() << endl;

    f.writeArray2D("A", A);
    f.close();

    DataFile g("test.h5");
    dvector w = g.readVector("v");
    for (size_t i=0; i<w.size(); i++) {
        cout << w[i] << endl;
    }

    cout << g.readScalar("x") << endl;
    cout << g.readArray2D("A") << endl;

    g.close();
}