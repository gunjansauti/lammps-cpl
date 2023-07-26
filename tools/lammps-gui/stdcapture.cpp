/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   LAMMPS development team: developers@lammps.org

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
   ------------------------------------------------------------------------- */

// adapted from: https://stackoverflow.com/questions/5419356/redirect-stdout-stderr-to-a-string

#include "stdcapture.h"

#ifdef _WIN32
#include <io.h>
#define popen _popen
#define pclose _pclose
#define stat _stat
#define dup _dup
#define dup2 _dup2
#define fileno _fileno
#define close _close
#define pipe _pipe
#define read _read
#define eof _eof
#else
#include <unistd.h>
#endif

#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <thread>

StdCapture::StdCapture() : m_capturing(false), m_oldStdOut(0)
{
    // make stdout unbuffered so that we don't need to flush the stream
    setvbuf(stdout, NULL, _IONBF, 0);

    m_pipe[READ]  = 0;
    m_pipe[WRITE] = 0;
#if _WIN32
    if (pipe(m_pipe, 65536, O_BINARY) == -1) return;
#else
    if (pipe(m_pipe) == -1) return;
#endif
    m_oldStdOut = dup(fileno(stdout));
    if (m_oldStdOut == -1) return;
}

StdCapture::~StdCapture()
{
    if (m_capturing) {
        EndCapture();
    }
    if (m_oldStdOut > 0) close(m_oldStdOut);
    if (m_pipe[READ] > 0) close(m_pipe[READ]);
    if (m_pipe[WRITE] > 0) close(m_pipe[WRITE]);
}

void StdCapture::BeginCapture()
{
    if (m_capturing) EndCapture();
    dup2(m_pipe[WRITE], fileno(stdout));
    m_capturing = true;
}

bool StdCapture::EndCapture()
{
    if (!m_capturing) return false;
    dup2(m_oldStdOut, fileno(stdout));
    m_captured.clear();

    constexpr int bufSize = 1025;
    char buf[bufSize];
    int bytesRead;
    bool fd_blocked;

    do {
        bytesRead  = 0;
        fd_blocked = false;

#ifdef _WIN32
        if (!eof(m_pipe[READ])) {
            bytesRead = read(m_pipe[READ], buf, bufSize - 1);
        }
#else
        bytesRead = read(m_pipe[READ], buf, bufSize - 1);
#endif
        if (bytesRead > 0) {
            buf[bytesRead] = 0;
            m_captured += buf;
        } else if (bytesRead < 0) {
            fd_blocked = ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR));

            if (fd_blocked) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } while (fd_blocked || (bytesRead == (bufSize - 1)));
    m_capturing = false;
    return true;
}

std::string StdCapture::GetCapture() const
{
    std::string::size_type idx = m_captured.find_last_not_of("\r\n");
    if (idx == std::string::npos) {
        return m_captured;
    } else {
        return m_captured.substr(0, idx + 1);
    }
}

// Local Variables:
// c-basic-offset: 4
// End:
