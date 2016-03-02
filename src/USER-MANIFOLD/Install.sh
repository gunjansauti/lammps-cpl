# Install/unInstall package files in LAMMPS
# mode = 0/1/2 for uninstall/install/update
#
# Copied from USER-MISC.
#

mode=$1

# arg1 = file, arg2 = file it depends on

action () {
  if (test $mode = 0) then
    rm -f ../$1
  elif (! cmp -s $1 ../$1) then
    if (test -z "$2" || test -e ../$2) then
      cp $1 ..
      if (test $mode = 2) then
        echo "  updating src/$1"
      fi
    fi
  elif (test ! -n "$2") then
    if (test ! -e ../$2) then
      rm -f ../$1
    fi
  fi
}

# all package files
# Factory needs to be remade after each modification/new manifold,
# other than that it is all good.

for file in *.cpp *.h; do
  action $file
  # manifold_factory.cpp
done
