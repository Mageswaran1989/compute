//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://kylelutz.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_PROGRAM_HPP
#define BOOST_COMPUTE_PROGRAM_HPP

#include <string>
#include <vector>
#include <fstream>
#include <streambuf>

#ifdef BOOST_COMPUTE_DEBUG_KERNEL_COMPILATION
#include <iostream>
#endif

#include <boost/compute/config.hpp>
#include <boost/compute/context.hpp>
#include <boost/compute/exception.hpp>
#include <boost/compute/detail/assert_cl_success.hpp>

#ifdef BOOST_COMPUTE_USE_OFFLINE_CACHE
#include <sstream>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <boost/compute/platform.hpp>
#include <boost/compute/detail/getenv.hpp>
#include <boost/compute/detail/sha1.hpp>
#endif

namespace boost {
namespace compute {

class kernel;

/// \class program
/// \brief A compute program.
///
/// The program class represents an OpenCL program.
///
/// Program objects are created with one of the static \c create_with_*
/// functions. For example, to create a program from a source string:
///
/// \snippet test/test_program.cpp create_with_source
///
/// And to create a program from a source file:
/// \code
/// boost::compute::program bar_program =
///     boost::compute::program::create_with_source_file("/path/to/bar.cl", context);
/// \endcode
///
/// Once a program object has been succesfully created, it can be compiled
/// using the build() method:
/// \code
/// // build the program
/// foo_program.build();
/// \endcode
///
/// Kernel objects can be created from compiled programs using the kernel
/// class's constructor:
/// \code
/// // create a kernel from the compiled program
/// boost::compute::kernel foo_kernel(foo_program, "foo");
/// \endcode
///
/// \see kernel
class program
{
public:
    /// Creates a null program object.
    program()
        : m_program(0)
    {
    }

    /// Creates a program object for \p program. If \p retain is \c true,
    /// the reference count for \p program will be incremented.
    explicit program(cl_program program, bool retain = true)
        : m_program(program)
    {
        if(m_program && retain){
            clRetainProgram(m_program);
        }
    }

    /// Creates a new program object as a copy of \p other.
    program(const program &other)
        : m_program(other.m_program)
    {
        if(m_program){
            clRetainProgram(m_program);
        }
    }

    /// Copies the program object from \p other to \c *this.
    program& operator=(const program &other)
    {
        if(this != &other){
            if(m_program){
                clReleaseProgram(m_program);
            }

            m_program = other.m_program;

            if(m_program){
                clRetainProgram(m_program);
            }
        }

        return *this;
    }

    #ifndef BOOST_COMPUTE_NO_RVALUE_REFERENCES
    /// Move-constructs a new program object from \p other.
    program(program&& other) BOOST_NOEXCEPT
        : m_program(other.m_program)
    {
        other.m_program = 0;
    }

    /// Move-assigns the program from \p other to \c *this.
    program& operator=(program&& other) BOOST_NOEXCEPT
    {
        if(m_program){
            clReleaseProgram(m_program);
        }

        m_program = other.m_program;
        other.m_program = 0;

        return *this;
    }
    #endif // BOOST_COMPUTE_NO_RVALUE_REFERENCES

    /// Destroys the program object.
    ~program()
    {
        if(m_program){
            BOOST_COMPUTE_ASSERT_CL_SUCCESS(
                clReleaseProgram(m_program)
            );
        }
    }

    /// Returns the underlying OpenCL program.
    cl_program& get() const
    {
        return const_cast<cl_program &>(m_program);
    }

    /// Returns the source code for the program.
    std::string source() const
    {
        return get_info<std::string>(CL_PROGRAM_SOURCE);
    }

    /// Returns the binary for the program.
    std::vector<unsigned char> binary() const
    {
        size_t binary_size = get_info<size_t>(CL_PROGRAM_BINARY_SIZES);
        std::vector<unsigned char> binary(binary_size);

        unsigned char *binary_ptr = &binary[0];
        cl_int error = clGetProgramInfo(m_program,
                                        CL_PROGRAM_BINARIES,
                                        sizeof(unsigned char **),
                                        &binary_ptr,
                                        0);
        if(error != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(error));
        }

        return binary;
    }

    std::vector<device> get_devices() const
    {
        std::vector<cl_device_id> device_ids =
            get_info<std::vector<cl_device_id> >(CL_PROGRAM_DEVICES);

        std::vector<device> devices;
        for(size_t i = 0; i < device_ids.size(); i++){
            devices.push_back(device(device_ids[i]));
        }

        return devices;
    }

    /// Returns the context for the program.
    context get_context() const
    {
        return context(get_info<cl_context>(CL_PROGRAM_CONTEXT));
    }

    /// Returns information about the program.
    ///
    /// \see_opencl_ref{clGetProgramInfo}
    template<class T>
    T get_info(cl_program_info info) const
    {
        return detail::get_object_info<T>(clGetProgramInfo, m_program, info);
    }

    /// \overload
    template<int Enum>
    typename detail::get_object_info_type<program, Enum>::type
    get_info() const;

    /// Returns build information about the program.
    ///
    /// For example, this function can be used to retreive the options used
    /// to build the program:
    /// \code
    /// std::string build_options =
    ///     program.get_build_info<std::string>(CL_PROGRAM_BUILD_OPTIONS);
    /// \endcode
    ///
    /// \see_opencl_ref{clGetProgramInfo}
    template<class T>
    T get_build_info(cl_program_build_info info, const device &device) const
    {
        return detail::get_object_info<T>(clGetProgramBuildInfo, m_program, info, device.id());
    }

    /// Builds the program with \p options.
    ///
    /// If the program fails to compile, this function will throw an
    /// opencl_error exception.
    /// \code
    /// try {
    ///     // attempt to compile to program
    ///     program.build();
    /// }
    /// catch(boost::compute::opencl_error &e){
    ///     // program failed to compile, print out the build log
    ///     std::cout << program.build_log() << std::endl;
    /// }
    /// \endcode
    ///
    /// \see_opencl_ref{clBuildProgram}
    void build(const std::string &options = std::string())
    {
        const char *options_string = 0;

        if(!options.empty()){
            options_string = options.c_str();
        }

        cl_int ret = clBuildProgram(m_program, 0, 0, options_string, 0, 0);

        #ifdef BOOST_COMPUTE_DEBUG_KERNEL_COMPILATION
        if(ret != CL_SUCCESS){
            // print the error, source code and build log
            std::cerr << "Boost.Compute: "
                      << "kernel compilation failed (" << ret << ")\n"
                      << "--- source ---\n"
                      << source()
                      << "\n--- build log ---\n"
                      << build_log()
                      << std::endl;
        }
        #endif

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }
    }

    #if defined(CL_VERSION_1_2) || defined(BOOST_COMPUTE_DOXYGEN_INVOKED)
    /// Compiles the program with \p options.
    ///
    /// \opencl_version_warning{1,2}
    ///
    /// \see_opencl_ref{clCompileProgram}
    void compile(const std::string &options = std::string())
    {
        const char *options_string = 0;

        if(!options.empty()){
            options_string = options.c_str();
        }

        cl_int ret = clCompileProgram(
            m_program, 0, 0, options_string, 0, 0, 0, 0, 0
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }
    }

    /// Links the programs in \p programs with \p options in \p context.
    ///
    /// \opencl_version_warning{1,2}
    ///
    /// \see_opencl_ref{clLinkProgram}
    static program link(const std::vector<program> &programs,
                        const context &context,
                        const std::string &options = std::string())
    {
        const char *options_string = 0;

        if(!options.empty()){
            options_string = options.c_str();
        }

        cl_int ret;
        cl_program program_ = clLinkProgram(
            context.get(),
            0,
            0,
            options_string,
            static_cast<uint_>(programs.size()),
            reinterpret_cast<const cl_program*>(&programs[0]),
            0,
            0,
            &ret
        );

        if(!program_){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return program(program_, false);
    }
    #endif // CL_VERSION_1_2

    /// Returns the build log.
    std::string build_log() const
    {
        return get_build_info<std::string>(CL_PROGRAM_BUILD_LOG, get_devices().front());
    }

    /// Creates and returns a new kernel object for \p name.
    ///
    /// For example, to create the \c "foo" kernel (after the program has been
    /// created and built):
    /// \code
    /// boost::compute::kernel foo_kernel = foo_program.create_kernel("foo");
    /// \endcode
    kernel create_kernel(const std::string &name) const;

    /// Returns \c true if the program is the same at \p other.
    bool operator==(const program &other) const
    {
        return m_program == other.m_program;
    }

    /// Returns \c true if the program is different from \p other.
    bool operator!=(const program &other) const
    {
        return m_program != other.m_program;
    }

    /// \internal_
    operator cl_program() const
    {
        return m_program;
    }

    /// Creates a new program with \p source in \p context.
    ///
    /// \see_opencl_ref{clCreateProgramWithSource}
    static program create_with_source(const std::string &source,
                                      const context &context)
    {
        const char *source_string = source.c_str();

        cl_int error = 0;
        cl_program program_ = clCreateProgramWithSource(context,
                                                        uint_(1),
                                                        &source_string,
                                                        0,
                                                        &error);
        if(!program_){
            BOOST_THROW_EXCEPTION(opencl_error(error));
        }

        return program(program_, false);
    }

    /// Creates a new program with \p file in \p context.
    ///
    /// \see_opencl_ref{clCreateProgramWithSource}
    static program create_with_source_file(const std::string &file,
                                           const context &context)
    {
        // open file stream
        std::ifstream stream(file.c_str());

        // read source
        std::string source(
            (std::istreambuf_iterator<char>(stream)),
            std::istreambuf_iterator<char>()
        );

        // create program
        return create_with_source(source, context);
    }

    /// Creates a new program with \p binary of \p binary_size in
    /// \p context.
    ///
    /// \see_opencl_ref{clCreateProgramWithBinary}
    static program create_with_binary(const unsigned char *binary,
                                      size_t binary_size,
                                      const context &context)
    {
        const cl_device_id device = context.get_device().id();

        cl_int error = 0;
        cl_int binary_status = 0;
        cl_program program_ = clCreateProgramWithBinary(context,
                                                        uint_(1),
                                                        &device,
                                                        &binary_size,
                                                        &binary,
                                                        &binary_status,
                                                        &error);
        if(!program_){
            BOOST_THROW_EXCEPTION(opencl_error(error));
        }
        if(binary_status != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(binary_status));
        }

        return program(program_, false);
    }

    /// Creates a new program with \p binary in \p context.
    ///
    /// \see_opencl_ref{clCreateProgramWithBinary}
    static program create_with_binary(const std::vector<unsigned char> &binary,
                                      const context &context)
    {
        return create_with_binary(&binary[0], binary.size(), context);
    }

    /// Creates a new program with \p file in \p context.
    ///
    /// \see_opencl_ref{clCreateProgramWithBinary}
    static program create_with_binary_file(const std::string &file,
                                           const context &context)
    {
        // open file stream
        std::ifstream stream(file.c_str(), std::ios::in | std::ios::binary);

        // read binary
        std::vector<unsigned char> binary(
            (std::istreambuf_iterator<char>(stream)),
            std::istreambuf_iterator<char>()
        );

        // create program
        return create_with_binary(&binary[0], binary.size(), context);
    }

    #if defined(CL_VERSION_1_2) || defined(BOOST_COMPUTE_DOXYGEN_INVOKED)
    /// Creates a new program with the built-in kernels listed in
    /// \p kernel_names for \p devices in \p context.
    ///
    /// \opencl_version_warning{1,2}
    ///
    /// \see_opencl_ref{clCreateProgramWithBuiltInKernels}
    static program create_with_builtin_kernels(const context &context,
                                               const std::vector<device> &devices,
                                               const std::string &kernel_names)
    {
        cl_int error = 0;

        cl_program program_ = clCreateProgramWithBuiltInKernels(
            context.get(),
            static_cast<uint_>(devices.size()),
            reinterpret_cast<const cl_device_id *>(&devices[0]),
            kernel_names.c_str(),
            &error
        );

        if(!program_){
            BOOST_THROW_EXCEPTION(opencl_error(error));
        }

        return program(program_, false);
    }
    #endif // CL_VERSION_1_2

    /// Create a new program with \p source in \p context and builds it with \p options.
    /**
     * In case BOOST_COMPUTE_USE_OFFLINE_CACHE macro is defined,
     * the compiled binary is stored for reuse in the offline cache located in
     * $HOME/.boost_compute on UNIX-like systems and in %APPDATA%/boost_compute
     * on Windows.
     */
    static program build_with_source(
            const std::string &source,
            const context     &context,
            const std::string &options = std::string()
            )
    {
#ifdef BOOST_COMPUTE_USE_OFFLINE_CACHE
        // Get hash string for the kernel.
        std::string hash;
        {
            device   d(context.get_device());
            platform p(d.get_info<cl_platform_id>(CL_DEVICE_PLATFORM));

            std::ostringstream src;
            src << "// " << p.name() << " v" << p.version() << "\n"
                << "// " << context.get_device().name() << "\n"
                << "// " << options << "\n\n"
                << source;

            hash = detail::sha1(src.str());
        }

        // Try to get cached program binaries:
        try {
            boost::optional<program> prog = load_program_binary(hash, context);

            if (prog) {
                prog->build(options);
                return *prog;
            }
        } catch (...) {
            // Something bad happened. Fallback to normal compilation.
        }

        // Cache is apparently not available. Just compile the sources.
#endif
        const char *source_string = source.c_str();

        cl_int error = 0;
        cl_program program_ = clCreateProgramWithSource(context,
                                                        uint_(1),
                                                        &source_string,
                                                        0,
                                                        &error);
        if(!program_){
            BOOST_THROW_EXCEPTION(opencl_error(error));
        }

        program prog(program_, false);
        prog.build(options);

#ifdef BOOST_COMPUTE_USE_OFFLINE_CACHE
        // Save program binaries for future reuse.
        save_program_binary(hash, prog);
#endif

        return prog;
    }

private:
#ifdef BOOST_COMPUTE_USE_OFFLINE_CACHE
    // Path delimiter symbol for the current OS.
    static const std::string& path_delim() {
        static const std::string delim =
            boost::filesystem::path("/").make_preferred().string();
        return delim;
    }

    // Path to appdata folder.
    static inline const std::string& appdata_path() {
#ifdef WIN32
        static const std::string appdata = detail::getenv("APPDATA")
            + path_delim() + "boost_compute";
#else
        static const std::string appdata = detail::getenv("HOME")
            + path_delim() + ".boost_compute";
#endif
        return appdata;
    }

    // Path to cached binaries.
    static std::string program_binary_path(const std::string &hash, bool create = false)
    {
        std::string dir = appdata_path()    + path_delim()
                        + hash.substr(0, 2) + path_delim()
                        + hash.substr(2);

        if (create) boost::filesystem::create_directories(dir);

        return dir + path_delim();
    }

    // Saves program binaries for future reuse.
    static void save_program_binary(const std::string &hash, const program &prog)
    {
        std::string fname = program_binary_path(hash, true) + "kernel";
        std::ofstream bfile(fname.c_str(), std::ios::binary);
        if (!bfile) return;

        std::vector<unsigned char> binary = prog.binary();

        size_t binary_size = binary.size();
        bfile.write((char*)&binary_size, sizeof(size_t));
        bfile.write((char*)binary.data(), binary_size);
    }

    // Tries to read program binaries from file cache.
    static boost::optional<program> load_program_binary(
            const std::string &hash, const context &ctx
            )
    {
        std::string fname = program_binary_path(hash) + "kernel";
        std::ifstream bfile(fname.c_str(), std::ios::binary);
        if (!bfile) return boost::optional<program>();

        size_t binary_size;
        std::vector<unsigned char> binary;

        bfile.read((char*)&binary_size, sizeof(size_t));

        binary.resize(binary_size);
        bfile.read((char*)binary.data(), binary_size);

        return boost::optional<program>(
                program::create_with_binary(
                    binary.data(), binary_size, ctx
                    )
                );
    }
#endif // BOOST_COMPUTE_USE_OFFLINE_CACHE

private:
    cl_program m_program;
};

/// \internal_ define get_info() specializations for program
BOOST_COMPUTE_DETAIL_DEFINE_GET_INFO_SPECIALIZATIONS(program,
    ((cl_uint, CL_PROGRAM_REFERENCE_COUNT))
    ((cl_context, CL_PROGRAM_CONTEXT))
    ((cl_uint, CL_PROGRAM_NUM_DEVICES))
    ((std::vector<cl_device_id>, CL_PROGRAM_DEVICES))
    ((std::string, CL_PROGRAM_SOURCE))
    ((std::vector<size_t>, CL_PROGRAM_BINARY_SIZES))
    ((std::vector<unsigned char *>, CL_PROGRAM_BINARIES))
)

#ifdef CL_VERSION_1_2
BOOST_COMPUTE_DETAIL_DEFINE_GET_INFO_SPECIALIZATIONS(program,
    ((size_t, CL_PROGRAM_NUM_KERNELS))
    ((std::string, CL_PROGRAM_KERNEL_NAMES))
)
#endif // CL_VERSION_1_2

} // end compute namespace
} // end boost namespace

#endif // BOOST_COMPUTE_PROGRAM_HPP
