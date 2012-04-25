#ifndef LISTCODERS_H__
#define LISTCODERS_H__

#include <cmath>
#include <cassert>

namespace Moses {

template <typename T = unsigned int>
class VarIntType {
  private:
    template <typename IntType, typename OutIt>
    static void encodeSymbol(IntType input, OutIt output) {
        if(input == 0) {
          *output = 0;
          output++;
          return;
        }
      
        T msb = 1 << (sizeof(T)*8-1);
        IntType mask  = ~msb;
        IntType shift = (sizeof(T)*8-1);
        
        while(input) {
            T res = input & mask;
            input >>= shift;
            if(input)
                res |= msb;
            *output = res;
            output++;
        }
    };
    
    template <typename InIt, typename IntType>
    static void decodeSymbol(InIt &it, InIt end, IntType &output) {
        T msb = 1 << (sizeof(T)*8-1);
        IntType shift = (sizeof(T)*8-1);
        
        output = 0;
        size_t i = 0;
        while(it != end && *it & msb) {
            IntType temp = *it & ~msb;
            temp <<= shift*i;
            output |= temp;
            it++; i++;
        }
        assert(it != end);
        
        IntType temp = *it;
        temp <<= shift*i;
        output |= temp;
        it++;
    }
    
    

  public:
        
    template <typename InIt, typename OutIt>
    static void encode(InIt it, InIt end, OutIt outIt) {
        while(it != end) {
            encodeSymbol(*it, outIt);
            it++;
        }
    }
    
    
    template <typename InIt, typename OutIt>
    static void decode(InIt &it, InIt end, OutIt outIt) {
        while(it != end) {
            size_t output;
            decodeSymbol(it, end, output);
            *outIt = output;
            outIt++;
        }
    }
    
    template <typename InIt>
    static size_t decodeAndSum(InIt &it, InIt end, size_t num) {
        size_t sum = 0;
        size_t curr = 0;
        
        while(it != end && curr < num) {
          size_t output;
          decodeSymbol(it, end, output);
          sum += output; curr++;
        }
        
        return sum;
    }

};

typedef VarIntType<unsigned char> VarByte;

typedef VarByte VarInt8;
typedef VarIntType<unsigned short> VarInt16;
typedef VarIntType<unsigned int>   VarInt32;

class Simple9 {
  private:
    typedef unsigned int uint;
        
    template <typename InIt>
    inline static void encodeSymbol(uint &output, InIt it, InIt end) {
        uint length = end - it;
      
        uint type;
        uint bitlength;
        
        switch(length) {
          case 1:  type = 1; bitlength = 28; break;
          case 2:  type = 2; bitlength = 14; break;
          case 3:  type = 3; bitlength = 9;  break;
          case 4:  type = 4; bitlength = 7;  break;
          case 5:  type = 5; bitlength = 5;  break;
          case 7:  type = 6; bitlength = 4;  break;
          case 9:  type = 7; bitlength = 3;  break;
          case 14: type = 8; bitlength = 2;  break;
          case 28: type = 9; bitlength = 1;  break;
        }
      
        output = 0;
        output |= (type << 28);
 
        uint i = 0;
        while(it != end) {
            uint l = bitlength * (length-i-1);
            output |= *it << l;
            it++;
            i++;
        }
    }
    
    template <typename OutIt>
    static inline void decodeSymbol(uint input, OutIt outIt) {
        uint type = (input >> 28);
        
        uint bitlen;
        uint shift;
        uint mask;
        
        switch(type) {
          case 1: bitlen = 28; shift = 0;  mask = 268435455; break;
          case 2: bitlen = 14; shift = 14; mask = 16383; break;
          case 3: bitlen = 9;  shift = 18; mask = 511; break;
          case 4: bitlen = 7;  shift = 21; mask = 127; break;
          case 5: bitlen = 5;  shift = 20; mask = 31; break;
          case 6: bitlen = 4;  shift = 24; mask = 15; break;
          case 7: bitlen = 3;  shift = 24; mask = 7; break;
          case 8: bitlen = 2;  shift = 26; mask = 3; break;
          case 9: bitlen = 1;  shift = 27; mask = 1; break;
        }
      
        while(shift > 0) {
          *outIt = (input >> shift) & mask;
          shift -= bitlen;  
          outIt++;
        }
        *outIt = input & mask;
        outIt++;  
    }
    
    static inline size_t decodeAndSumSymbol(uint input, size_t num, size_t &curr) {
        uint type = (input >> 28);
        
        uint bitlen;
        uint shift;
        uint mask;
        
        switch(type) {
          case 1: bitlen = 28; shift = 0;  mask = 268435455; break;
          case 2: bitlen = 14; shift = 14; mask = 16383; break;
          case 3: bitlen = 9;  shift = 18; mask = 511; break;
          case 4: bitlen = 7;  shift = 21; mask = 127; break;
          case 5: bitlen = 5;  shift = 20; mask = 31; break;
          case 6: bitlen = 4;  shift = 24; mask = 15; break;
          case 7: bitlen = 3;  shift = 24; mask = 7; break;
          case 8: bitlen = 2;  shift = 26; mask = 3; break;
          case 9: bitlen = 1;  shift = 27; mask = 1; break;
        }

        size_t sum = 0;
        while(shift > 0) {
          sum += (input >> shift) & mask;
          shift -= bitlen;
          if(++curr == num)
            return sum;
        }
        sum += input & mask;
        curr++;
        return sum;
    }
    
  public:
    template <typename InIt, typename OutIt>
    static void encode(InIt it, InIt end, OutIt outIt) {        
        uint parts[] = { 1, 2, 3, 4, 5, 7, 9, 14, 28 };

        uint buffer[28];
        for(InIt i = it; i < end; i++) {
            uint lastbit = 1;
            uint lastpos = 0;
            uint lastyes = 0;
            uint j = 0;
            
            double log2 = log(2);
            while(j < 9 && lastpos < 28 && (i+lastpos) < end) {
                if(lastpos >= parts[j]) 
                    j++;
                
                buffer[lastpos] = *(i + lastpos);
                
                uint reqbit = ceil(log(buffer[lastpos]+1)/log2);
                assert(reqbit <= 28);
                
                uint bit = 28/floor(28/reqbit);
                if(lastbit < bit)
                    lastbit = bit;
                    
                if(parts[j] > 28/lastbit)
                    break;
                else if(lastpos == parts[j]-1)
                    lastyes = lastpos;
                    
                lastpos++;
            }
            i += lastyes;       
            
            uint length = lastyes + 1;
            uint output;
            encodeSymbol(output, buffer, buffer + length);
            
            *outIt = output;
            outIt++;
        }
    }
    
    template <typename InIt, typename OutIt>
    static void decode(InIt &it, InIt end, OutIt outIt) {
        while(it != end) {
            decodeSymbol(*it, outIt);
            it++;
        }
    }
    
    template <typename InIt>
    static size_t decodeAndSum(InIt &it, InIt end, size_t num) {
        size_t sum = 0;
        size_t curr = 0;
        while(it != end && curr < num) {
            sum += decodeAndSumSymbol(*it, num, curr);
            it++;
        }
        assert(curr == num);
        return sum;
    }
};

}

#endif