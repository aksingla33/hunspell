#include <hunspell.hxx>
#include <iconv.h>
#include <errno.h>
#include <Rcpp.h>

#ifdef _WIN32
#define ICONV_CONST_FIX (const char**)
#else
#define ICONV_CONST_FIX
#endif

class hunspell_dict {
  Hunspell * pMS_;
  iconv_t cd_from_;
  iconv_t cd_to_;
  std::string enc_;

private:
  iconv_t new_iconv(const char * from, const char * to){
    iconv_t cd = iconv_open(to, from);
    if(cd == (iconv_t) -1){
      switch(errno){
        case EINVAL: throw std::runtime_error(std::string("Unsupported iconv conversion: ") + from + "to" + to);
        default: throw std::runtime_error("General error in iconv_open()");
      }
    }
    return cd;
  }

public:
  // Some strings are regular strings
  hunspell_dict(std::string affix, Rcpp::CharacterVector dicts){
    std::string dict(dicts[0]);
    pMS_ = new Hunspell(affix.c_str(), dict.c_str());
    if(!pMS_)
      throw std::runtime_error(std::string("Failed to load file ") + dict);

    //add additional dictionaries if more than one
    //assuming the same affix?? This can cause unpredictable behavior
    for(int i = 1; i < dicts.length(); i++)
      pMS_->add_dic(std::string(dicts[0]).c_str());

    enc_ = pMS_->get_dict_encoding();
    cd_from_ = new_iconv("UTF-8", enc_.c_str());
    cd_to_ = new_iconv(enc_.c_str(), "UTF-8");
  }

  ~hunspell_dict() {
    try {
      iconv_close(cd_from_);
      iconv_close(cd_to_);
      delete pMS_;
    } catch (...) {}
  }

  unsigned short * get_wordchars_utf16(int *len){
    return (unsigned short *) pMS_->get_wordchars_utf16().data();
  }

  bool spell(std::string str){
    return pMS_->spell(str);
  }

  bool spell(Rcpp::String word){
    char * str = string_from_r(word);
    // Words that cannot be converted into the required encoding are by definition incorrect
    if(str == NULL)
      return false;
    bool res = pMS_->spell(std::string(str));
    free(str);
    return res;
  }

  void add_word(Rcpp::String word){
    char * str = string_from_r(word);
    if(str != NULL) {
      pMS_->add(str);
      free(str);
    }
  }

  std::string enc(){
    return enc_;
  }

  bool is_utf8(){
    return (
      !strcmp(enc_.c_str(), "UTF-8") || !strcmp(enc_.c_str(), "utf8") ||
      !strcmp(enc_.c_str(), "UTF8") ||!strcmp(enc_.c_str(), "utf-8")
    );
  }

  Rcpp::CharacterVector suggest(Rcpp::String word){
    char * str = string_from_r(word);
    Rcpp::CharacterVector out;
    for (const auto& x : pMS_->suggest(str)) {
      out.push_back(x);
    }
    free(str);
    return out;
  }

  Rcpp::CharacterVector analyze(Rcpp::String word){
    Rcpp::CharacterVector out;
    char * str = string_from_r(word);
    for (const auto& x : pMS_->analyze(str)) {
      out.push_back(x);
    }
    free(str);
    return out;
  }

  Rcpp::CharacterVector stem(Rcpp::String word){
    Rcpp::CharacterVector out;
    char * str = string_from_r(word);
    for (const auto& x : pMS_->stem(str)) {
      out.push_back(x);
    }
    free(str);
    return out;
  }

  //adds ignore words to the dictionary
  void add_words(Rcpp::StringVector words){
    for(int i = 0; i < words.length(); i++){
      add_word(words[i]);
    }
  }

  iconv_t cd_from(){
    return cd_from_;
  }

  iconv_t cd_to(){
    return cd_to_;
  }

  std::string wc(){
    return pMS_->get_wordchars();
  }

  Rcpp::String r_wordchars(){
    if(is_utf8()){
      const std::vector<w_char>& vec_wordchars_utf16 = pMS_->get_wordchars_utf16();
      int wordchars_utf16_len = vec_wordchars_utf16.size();
      const w_char* wordchars_utf16 = wordchars_utf16_len ? &vec_wordchars_utf16[0] : NULL;
      return string_to_r((char*) wordchars_utf16);
    } else {
      return string_to_r((char*) pMS_->get_wordchars().c_str());
    }
  }

  std::vector<w_char> get_wordchars_utf16(){
    return pMS_->get_wordchars_utf16();
  }

  char * string_from_r(Rcpp::String str){
    str.set_encoding(CE_UTF8);
    char * inbuf = (char *) str.get_cstring();
    size_t inlen = strlen(inbuf);
    size_t outlen = 4 * inlen + 1;
    char * output = (char *) malloc(outlen);
    char * cur = output;
    size_t success = iconv(cd_from_, ICONV_CONST_FIX &inbuf, &inlen, &cur, &outlen);
    if(success == (size_t) -1){
      free(output);
      return NULL;
    }
    *cur = '\0';
    output = (char *) realloc(output, outlen + 1);
    return output;
  }

  Rcpp::String string_to_r(char * inbuf){
    if(inbuf == NULL)
      return NA_STRING;
    size_t inlen = strlen(inbuf);
    size_t outlen = 4 * inlen + 1;
    char * output = (char *) malloc(outlen);
    char * cur = output;
    size_t success = iconv(cd_to_, ICONV_CONST_FIX &inbuf, &inlen, &cur, &outlen);
    if(success == (size_t) -1){
      free(output);
      return NA_STRING;
    }
    *cur = '\0';
    Rcpp::String res = Rcpp::String(output);
    res.set_encoding(CE_UTF8);
    free(output);
    return res;
  }
};
