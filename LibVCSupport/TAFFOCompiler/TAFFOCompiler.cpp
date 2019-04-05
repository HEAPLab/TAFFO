/* Copyright 2019 Politecnico di Milano.
 * Developed by : Daniele Cattaneo
 *
 * This file is part of libVersioningCompiler
 *
 * libVersioningCompiler is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * libVersioningCompiler is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libVersioningCompiler. If not, see <http://www.gnu.org/licenses/>
 */
#include "TAFFOCompiler.hpp"

using namespace vc;


TAFFOCompiler::Component TAFFOCompiler::Init = {"TaffoInitializer", "-taffoinit", "INITLIB"};
TAFFOCompiler::Component TAFFOCompiler::VRA = {"TaffoVRA", "-taffoVRA", "VRALIB"};
TAFFOCompiler::Component TAFFOCompiler::DTA = {"TaffoDTA", "-taffodta", "TUNERLIB"};
TAFFOCompiler::Component TAFFOCompiler::Conversion = {"LLVMFloatToFixed", "-flttofix", "PASSLIB"};


TAFFOCompiler::TAFFOCompiler(
  const std::string &compilerID,
  const std::string &llvmOptPath,
  const std::string &llvmClangPath,
  const std::string &llvmLinkerPath,
  const std::string &taffoInstallPrefix,
  const std::string &libWorkingDir,
  const std::string &log) :
  Compiler(compilerID, "", libWorkingDir, log, "", true),
  llvmOptPath(llvmOptPath), llvmClangPath(llvmClangPath),
  llvmLinkerPath(llvmLinkerPath), taffoInstallPrefix(taffoInstallPrefix)
{
  if (const char *e_llvmdir = getenv("LLVM_DIR")) {
    if (this->llvmOptPath.size() == 0)
      this->llvmOptPath = std::string(e_llvmdir) + "/bin/opt";
    if (this->llvmClangPath.size() == 0)
      this->llvmClangPath = std::string(e_llvmdir) + "/bin/clang";
    if (this->llvmLinkerPath.size() == 0)
      this->llvmLinkerPath = std::string(e_llvmdir) + "/bin/clang";
  }
}


TAFFOCompiler::TAFFOCompiler() :
  TAFFOCompiler(
    "taffo",
    "/usr/bin/opt",
    "/usr/bin/clang",
    "/usr/bin/clang",
    "",
    ".",
    "")
{ }


bool TAFFOCompiler::hasOptimizer() const
{
  return false;
}


std::string TAFFOCompiler::getLLVMLibExtension() const
{
  #ifdef __APPLE__
    return "dylib";
  #else
    return "so";
  #endif
}


std::string TAFFOCompiler::getInvocation(const TAFFOCompiler::Component &c) const
{
  std::string path;
  if (taffoInstallPrefix.size() > 0) {
    path = taffoInstallPrefix + "/lib/" + c.libName + "." + getLLVMLibExtension();
  } else if (const char *envpath = getenv(c.envName.c_str())){
    path = envpath;
  }
  if (exists(path))
    return llvmOptPath + " -load " + path + " " + c.optParamName;
  return "";
}


void TAFFOCompiler::splitOptimizationOptions(const opt_list_t& in, opt_list_t& outOpt, opt_list_t& outRest)
{
  for (const auto opt: in) {
    if (opt.getPrefix() == "-O")
      outOpt.push_back(opt);
    else
      outRest.push_back(opt);
  }
}


std::string TAFFOCompiler::generateIR(
  const std::vector<std::string> &src,
  const std::vector<std::string> &func,
  const std::string &versionID,
  const opt_list_t options) const
{
  opt_list_t optOpts;
  opt_list_t normalOpts;
  splitOptimizationOptions(options, optOpts, normalOpts);
  
  std::string raw_bitcode = Compiler::getBitcodeFileName(versionID + "_1_clang");
  std::string raw_cmd = llvmClangPath + " -c -emit-llvm -O0 -o \"" + raw_bitcode + "\" ";
  for (const auto &src_file : src) {
    raw_cmd = raw_cmd + " \"" + src_file + "\"";
  }
  for (auto &o : normalOpts) {
    raw_cmd = raw_cmd + " " + getOptionString(o);
  }
  Compiler::log_exec(raw_cmd);
  if (!exists(raw_bitcode))
    return "";
  
  std::string init_bitcode = Compiler::getBitcodeFileName(versionID + "_2_init");
  std::string init_cmd = getInvocation(Init) + " -o \"" + init_bitcode + "\" \"" + raw_bitcode + "\"";
  Compiler::log_exec(init_cmd);
  if (!exists(init_bitcode))
    return "";
  
  std::string vra_bitcode = Compiler::getBitcodeFileName(versionID + "_3_vra");
  std::string vra_cmd = getInvocation(VRA) + " -o \"" + vra_bitcode + "\" \"" + init_bitcode + "\"";
  Compiler::log_exec(vra_cmd);
  if (!exists(vra_bitcode))
    return "";
  
  std::string dta_bitcode = Compiler::getBitcodeFileName(versionID + "_4_dta");
  std::string dta_cmd = getInvocation(DTA) + " -o \"" + dta_bitcode + "\" \"" + vra_bitcode + "\"";
  Compiler::log_exec(dta_cmd);
  if (!exists(dta_bitcode))
    return "";
  
  std::string conv_bitcode;
  /* skip final clang pass when there are no options to process */
  if (optOpts.size() > 0) {
    conv_bitcode = Compiler::getBitcodeFileName(versionID + "_5_conv");
  } else {
    conv_bitcode = Compiler::getBitcodeFileName(versionID);
  }
  std::string conv_cmd = getInvocation(Conversion) + " -o \"" + conv_bitcode + "\" \"" + dta_bitcode + "\"";
  Compiler::log_exec(conv_cmd);
  if (!exists(conv_bitcode))
    return "";
  if (optOpts.size() == 0)
    return conv_bitcode;
  
  std::string end_bitcode = Compiler::getBitcodeFileName(versionID);
  std::string end_cmd = llvmClangPath + " -c -emit-llvm -o \"" + conv_bitcode + "\" \"" + dta_bitcode + "\"";
  for (auto &o : optOpts) {
    end_cmd = end_cmd + " " + getOptionString(o);
  }
  Compiler::log_exec(end_cmd);
  if (!exists(end_bitcode))
    return "";
  
  return end_bitcode;
}


std::string TAFFOCompiler::runOptimizer(
  const std::string &src_IR,
  const std::string &versionID,
  const opt_list_t options) const
{
  std::string error = __STRING(__FUNCTION__) ": ";
  error = error + "TAFFO compiler does not support optimizer";
  Compiler::unsupported(error);
  return "";
}


std::string TAFFOCompiler::generateBin(
  const std::vector<std::string> &src,
  const std::vector<std::string> &func,
  const std::string &versionID,
  const opt_list_t options) const
{
  std::string bitcode = generateIR(src, func, versionID, options);
  if (bitcode.empty())
    return "";
  
  // system call - command construction
  std::string binaryFile = Compiler::getSharedObjectFileName(versionID);
  std::string command = llvmLinkerPath + " -fpic -shared -o \"" + binaryFile + "\" \"" + bitcode + "\"";
  log_exec(command);
  if (!exists(binaryFile))
    return "";
  
  return binaryFile;
}


inline std::string TAFFOCompiler::getOptionString(const Option &o) const
{
  std::string tmp_val = o.getValue();
  if (tmp_val.length() > 2 &&
      // escape whitespaces with double quotes
      (tmp_val.find(" ") != std::string::npos ||
       tmp_val.find("\t") != std::string::npos) &&
      // ...but not if already within quotes
      !(tmp_val[0] == tmp_val[tmp_val.length() - 1] && tmp_val[0] == '\"'))
  {
    tmp_val = "\"" + tmp_val + "\"";
  }
  return o.getPrefix() + tmp_val;
}



