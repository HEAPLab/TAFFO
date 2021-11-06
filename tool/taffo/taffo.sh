#!/usr/bin/env bash

trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM

LOG=/dev/null

taffo_setenv_find()
{
  cd $1
  BASEDIR=$(pwd)

  if [[ $2 == 'lib' ]]; then
    SOEXT="so"
    if [ $(uname -s) = "Darwin" ]; then
      SOEXT="dylib";
    fi
    FN="$3.$SOEXT"
  else
    FN="$3"
  fi

  PATH="$BASEDIR/$2/$FN"

  if [[ ! -e "$PATH" ]]; then
    echo "Cannot find $FN" >> $LOG
  else
    echo "Found $PATH" >> $LOG
    echo "$PATH"
  fi
}


SCRIPTPATH=$(dirname "$BASH_SOURCE")
TAFFO_PREFIX=${SCRIPTPATH}/..

export TAFFOLIB=$(taffo_setenv_find $TAFFO_PREFIX 'lib' 'Taffo')
export INSTMIX=$(taffo_setenv_find $TAFFO_PREFIX 'bin' 'taffo-instmix')
export TAFFO_MLFEAT=$(taffo_setenv_find $TAFFO_PREFIX 'bin' 'taffo-mlfeat')
export TAFFO_FE=$(taffo_setenv_find $TAFFO_PREFIX 'bin' 'taffo-fe')
export TAFFO_PE=$(taffo_setenv_find $TAFFO_PREFIX 'bin' 'taffo-pe')

if [[ -z "$LLVM_DIR" ]]; then
  LLVM_DIR=$(llvm-config --prefix 2> /dev/null)
  if [[ $? -ne 0 ]]; then
    printf "*** WARNING ***\nCannot set LLVM_DIR using llvm-config\n"
  fi
fi
if [[ ! ( -z "$LLVM_DIR" ) ]]; then
  if [ $(uname -s) = "Darwin" ]; then
    # xcrun patches the command line parameters to clang to add the standard
    # include paths depending on where the currently active platform SDK is
    export CLANG="xcrun $LLVM_DIR/bin/clang"
    export CLANGXX="xcrun $LLVM_DIR/bin/clang++"
  fi
fi

if [[ -z $LLVM_DIR ]]; then
  echo -e '\033[33m'"Warning"'\033[39m'" using default llvm/clang";
else
  llvmbin="$LLVM_DIR/bin/";
fi
if [[ -z "$CLANG" ]]; then CLANG=${llvmbin}clang; fi
if [[ -z "$CLANGXX" ]]; then CLANGXX=${CLANG}++; fi
if [[ -z "$OPT" ]]; then OPT=${llvmbin}opt; fi
if [[ -z "$LLC" ]]; then LLC=${llvmbin}llc; fi
if [[ -z "$LLVM_LINK" ]]; then LLVM_LINK=${llvmbin}llvm-link; fi

llvm_debug=$(($("$OPT" --version | grep DEBUG | wc -l)))

parse_state=0
raw_opts="$@"
input_files=()
output_file="a"
float_output_file=
emit_source=
optimization=
opts=
float_opts=
init_flags=
vra_flags=
disable_vra=0
dta_flags=
dta_inst_set=
conversion_flags=
enable_errorprop=0
errorprop_flags=
errorprop_out=
mem2reg=-mem2reg
dontlink=
iscpp=$CLANG
feedback=0
pe_model_file=
temporary_dir=$(mktemp -d)
del_temporary_dir=1
help=0
for opt in $raw_opts; do
  case $parse_state in
    0)
      case $opt in
        -Xinit)
          parse_state=2;
          ;;
        -Xvra)
          parse_state=5;
          ;;
        -Xdta)
          parse_state=3;
          ;;
        -Xconversion)
          parse_state=4;
          ;;
        -Xerr)
          enable_errorprop=1
          parse_state=8;
          ;;
        -o*)
          if [[ ${#opt} -eq 2 ]]; then
            parse_state=1;
          else
            output_file=`echo "$opt" | cut -b 2`;
          fi;
          ;;
        -float-output)
          parse_state=6;
          ;;
        -O*)
          optimization=$opt;
          ;;
        -fixp*)
          conversion_flags="$conversion_flags $opt";
          ;;
        -c)
          dontlink="$opt";
          ;;
        -debug)
          if [[ $llvm_debug -ne 0 ]]; then
            init_flags="$init_flags -debug";
            dta_flags="$dta_flags -debug";
            conversion_flags="$conversion_flags -debug";
            vra_flags="$vra_flags -debug";
            errorprop_flags="$errorprop_flags -debug";
          fi
          LOG=/dev/stderr
          ;;
        -debug-taffo)
          if [[ $llvm_debug -ne 0 ]]; then
            init_flags="$init_flags --debug-only=taffo-init";
            dta_flags="$dta_flags --debug-only=taffo-dta";
            conversion_flags="$conversion_flags --debug-only=taffo-conversion";
            vra_flags="$vra_flags --debug-only=taffo-vra";
            errorprop_flags="$errorprop_flags --debug-only=errorprop";
          fi
          LOG=/dev/stderr
          ;;
        -disable-vra)
          disable_vra=1
          ;;
        -enable-err)
          enable_errorprop=1
          ;;
        -feedback)
          feedback=1
          ;;
        -pe-model)
          parse_state=7
          ;;
        -temp-dir)
          del_temporary_dir=0
          parse_state=9
          ;;
        -err-out)
          enable_errorprop=1
          parse_state=10
          ;;
        -no-mem2reg)
          mem2reg=
          ;;
        -mixedmode)
          dta_flags="$dta_flags -mixedmode=1"
          if [[ -z "$dta_inst_set" ]]; then
            dta_inst_set="-instructionsetfile=$TAFFO_PREFIX/share/ILP/constrain/fix"
          fi
          ;;
        -costmodel)
          parse_state=11
          ;;
        -costmodelfilename*)
          dta_flags="$dta_flags $opt"
          ;;
        -instructionsetfile*)
          dta_inst_set="$opt"
          ;;
        -S)
          emit_source="s"
          float_opts="-S"
          ;;
        -emit-llvm)
          emit_source="ll"
          float_opts="$float_opts -S -emit-llvm"
          ;;
        -print-clang)
          printf '%s\n' "$CLANG"
          exit 0
          ;;
        -help | -h | -version | -v | --help | --version)
          help=1
          ;;
        -*)
          opts="$opts $opt";
          ;;
        *.c | *.ll)
          input_files+=( "$opt" );
          ;;
        *.cpp | *.cc)
          input_files+=( "$opt" );
          iscpp=$CLANGXX
          ;;
        *)
          opts="$opts $opt";
          ;;
      esac;
      ;;
    1)
      output_file="$opt";
      parse_state=0;
      ;;
    2)
      init_flags="$init_flags $opt";
      parse_state=0;
      ;;
    3)
      dta_flags="$dta_flags $opt";
      parse_state=0;
      ;;
    4)
      conversion_flags="$conversion_flags $opt";
      parse_state=0;
      ;;
    5)
      vra_flags="$vra_flags $opt";
      parse_state=0;
      ;;
    6)
      float_output_file="$opt";
      parse_state=0;
      ;;
    7)
      pe_model_file="$opt";
      parse_state=0;
      ;;
    8)
      errorprop_flags="$errorprop_flags $opt";
      parse_state=0;
      ;;
    9)
      temporary_dir="$opt";
      parse_state=0;
      ;;
    10)
      errorprop_out="$opt";
      parse_state=0;
      ;;
    11)
      f="$TAFFO_PREFIX"/share/ILP/cost/"$opt".csv
      if [[ -e "$f" ]]; then
        dta_flags="$dta_flags -costmodelfilename=$f"
      else
        printf 'error: specified cost model "%s" does not exist\n' "$opt"
        exit 1
      fi
      parse_state=0
      ;;
  esac;
done

output_basename=$(basename ${output_file})

if [[ ( -z "$input_files" ) || ( $help -ne 0 ) ]]; then
  cat << HELP_END
TAFFO: Tuning Assistant for Floating point and Fixed point Optimization
Usage: taffo [options] file...

The specified files can be any C or C++ source code files.
Apart from the TAFFO-specific options listed below, all CLANG options are
also accepted.

Options:
  -o <file>             Write compilation output to <file>
  -O<level>             Set the optimization level to the specified value.
                        The accepted optimization levels are the same as CLANG.
                        (-O, -O1, -O2, -O3, -Os, -Of)
  -c                    Do not link the output
  -S                    Produce an assembly file in output instead of a binary
                        (overrides -c and -emit-llvm)
  -emit-llvm            Produce a LLVM-IR assembly file in output instead of
                        a binary (overrides -c and -S)
  -mixedmode            Enables experimental data-driven type allocator
    -costmodel <name>          Loads one of the builtin cost models
    -costmodelfilename=<file>  Loads the given the cost model file
                               (produced by taffo-costmodel)
    -instructionsetfile=<file> Loads the given instruction whitelist file
  -enable-err           Enable the error propagator (disabled by default)
  -err-out <file>       Produce a textual report about the estimates performed
                        by the Error Propagator in the specified file.
  -disable-vra          Disables the VRA analysis pass, and replaces it with
                        a simpler, optimistic, and potentially incorrect greedy
                        algorithm.
  -no-mem2reg           Disable scheduling of the mem2reg pass.
  -float-output <file>  Also compile the files without using TAFFO and store
                        the output to the specified location.
  -Xinit <option>       Pass the specified option to the Initializer pass of
                        TAFFO
  -Xvra <option>        Pass the specified option to the VRA pass of TAFFO
  -Xdta <option>        Pass the specified option to the DTA pass of TAFFO
  -Xconversion <option> Pass the specified option to the Conversion pass of
                        TAFFO
  -Xerr <option>        Pass the specified option to the Error Propagator pass
                        of TAFFO
  -debug                Enable LLVM and TAFFO debug logging during the
                        compilation.
  -debug-taffo          Enable TAFFO-only debug logging during the compilation.
  -temp-dir <dir>       Store various temporary files related to the execution
                        of TAFFO to the specified directory.

Available builtin cost models:
HELP_END
  for f in "$TAFFO_PREFIX"/share/ILP/cost/*.csv; do
    fn=$(basename "$f")
    printf '  %s\n' ${fn%%.csv}
  done
  exit 0
fi

# enable bash logging
if [[ $LOG != /dev/null ]]; then
  set -x
fi

###
###  Produce base .ll
###
if [[ ${#input_files[@]} -eq 1 ]]; then
  # one input file
  ${CLANG} \
    $opts -O0 -Xclang -disable-O0-optnone \
    -c -emit-llvm \
    ${input_files} \
    -S -o "${temporary_dir}/${output_basename}.1.taffotmp.ll" || exit $?
else
  # > 1 input files
  tmp=()
  for input_file in "${input_files[@]}"; do
    thisfn=$(basename "$input_file")
    thisfn=${thisfn%.*}
    thisfn="${temporary_dir}/${output_basename}.${thisfn}.0.taffotmp.ll"
    tmp+=( $thisfn )
    ${CLANG} \
      $opts -O0 -Xclang -disable-O0-optnone \
      -c -emit-llvm \
      ${input_file} \
      -S -o "${thisfn}" || exit $?
  done
  ${LLVM_LINK} \
    ${tmp[@]} \
    -S -o "${temporary_dir}/${output_basename}.1.taffotmp.ll" || exit $?
fi

# precompute clang invocation for compiling float version
build_float="${iscpp} $opts ${optimization} ${temporary_dir}/${output_basename}.1.taffotmp.ll"

###
###  TAFFO initialization
###
${OPT} \
  -load "$TAFFOLIB" \
  -taffoinit \
  ${init_flags} \
  -S -o "${temporary_dir}/${output_basename}.2.taffotmp.ll" "${temporary_dir}/${output_basename}.1.taffotmp.ll" || exit $?
  
###
###  TAFFO Value Range Analysis
###
if [[ $disable_vra -eq 0 ]]; then
  ${OPT} \
    -load "$TAFFOLIB" \
    ${mem2reg} -taffoVRA \
    ${vra_flags} \
    -S -o "${temporary_dir}/${output_basename}.3.taffotmp.ll" "${temporary_dir}/${output_basename}.2.taffotmp.ll" || exit $?;
else
  cp "${temporary_dir}/${output_basename}.2.taffotmp.ll" "${temporary_dir}/${output_basename}.3.taffotmp.ll";
fi

feedback_stop=0
if [[ $feedback -ne 0 ]]; then
  # init the feedback estimator if needed
  base_dta_flags="${dta_flags}"
  dta_flags="${base_dta_flags} "$($TAFFO_FE --init --state "${temporary_dir}/${output_basename}.festate.taffotmp.bin")
fi
while [[ $feedback_stop -eq 0 ]]; do
  ###
  ###  TAFFO Data Type Allocation
  ###
  ${OPT} \
    -load "$TAFFOLIB" \
    -taffodta -globaldce \
    ${dta_flags} ${dta_inst_set} \
    -S -o "${temporary_dir}/${output_basename}.4.taffotmp.ll" "${temporary_dir}/${output_basename}.3.taffotmp.ll" || exit $?
    
  ###
  ###  TAFFO Conversion
  ###
  ${OPT} \
    -load "$TAFFOLIB" \
    -flttofix -globaldce -dce \
    ${conversion_flags} \
    -S -o "${temporary_dir}/${output_basename}.5.taffotmp.ll" "${temporary_dir}/${output_basename}.4.taffotmp.ll" || exit $?
    
  ###
  ###  TAFFO Feedback Estimator
  ###
  if [[ ( $enable_errorprop -eq 1 ) || ( $feedback -ne 0 ) ]]; then
    ${OPT} \
      -load "$TAFFOLIB" \
      -errorprop \
      ${errorprop_flags} \
      -S -o "${temporary_dir}/${output_basename}.6.taffotmp.ll" "${temporary_dir}/${output_basename}.5.taffotmp.ll" 2> "${temporary_dir}/${output_basename}.errorprop.taffotmp.txt" || exit $?
    if [[ ! ( -z "$errorprop_out" ) ]]; then
      cp "${temporary_dir}/${output_basename}.errorprop.taffotmp.txt" "$errorprop_out"
    fi
  fi
  if [[ $feedback -eq 0 ]]; then
    break
  fi
  ${build_float} -S -emit-llvm \
    -o "${temporary_dir}/${output_basename}.float.taffotmp.ll" || exit $?
  ${TAFFO_PE} \
    --fix "${temporary_dir}/${output_basename}.5.taffotmp.ll" \
    --flt "${temporary_dir}/${output_basename}.float.taffotmp.ll" \
    --model ${pe_model_file} > "${temporary_dir}/${output_basename}.perfest.taffotmp.txt" || exit $?
  newflgs=$(${TAFFO_FE} \
    --pe-out "${temporary_dir}/${output_basename}.perfest.taffotmp.txt" \
    --ep-out "${temporary_dir}/${output_basename}.errorprop.taffotmp.txt" \
    --state "${temporary_dir}/${output_basename}.festate.taffotmp.bin" || exit $?)
  if [[ ( "$newflgs" == 'STOP' ) || ( -z "$newflgs" ) ]]; then
    feedback_stop=1
  else
    dta_flags="${base_dta_flags} ${newflgs}"
  fi
done

###
###  Backend
###

# Produce the requested output file
if [[ ( $emit_source == "s" ) || ( $del_temporary_dir -eq 0 ) ]]; then
  ${CLANG} \
    $opts ${optimization} \
    -c \
    "${temporary_dir}/${output_basename}.5.taffotmp.ll" \
    -S -o "${temporary_dir}/${output_basename}.taffotmp.s" || exit $?
fi
if [[ $emit_source == "s" ]]; then
  cp "${temporary_dir}/${output_basename}.taffotmp.s" "$output_file"
elif [[ $emit_source == "ll" ]]; then
  cp "${temporary_dir}/${output_basename}.5.taffotmp.ll" "$output_file"
else
  ${iscpp} \
    $opts ${optimization} \
    ${dontlink} \
    "${temporary_dir}/${output_basename}.5.taffotmp.ll" \
    -o "$output_file" || exit $?
fi

if [[ ! ( -z ${float_output_file} ) ]]; then
  ${build_float} \
    ${dontlink} ${float_opts} \
    -o "$float_output_file" || exit $?
fi

###
###  Cleanup
###
if [[ $del_temporary_dir -ne 0 ]]; then
  rm -rf "${temporary_dir}"
fi

