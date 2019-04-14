/*** Loads ***/

extern uint8_t _ITM_CALL_CONVENTION _ITM_RU1(TX_ARGS const uint8_t *);
extern uint8_t _ITM_CALL_CONVENTION _ITM_RaRU1(TX_ARGS const uint8_t *);
extern uint8_t _ITM_CALL_CONVENTION _ITM_RaWU1(TX_ARGS const uint8_t *);
extern uint8_t _ITM_CALL_CONVENTION _ITM_RfWU1(TX_ARGS const uint8_t *);

extern uint16_t _ITM_CALL_CONVENTION _ITM_RU2(TX_ARGS const uint16_t *);
extern uint16_t _ITM_CALL_CONVENTION _ITM_RaRU2(TX_ARGS const uint16_t *);
extern uint16_t _ITM_CALL_CONVENTION _ITM_RaWU2(TX_ARGS const uint16_t *);
extern uint16_t _ITM_CALL_CONVENTION _ITM_RfWU2(TX_ARGS const uint16_t *);

extern uint32_t _ITM_CALL_CONVENTION _ITM_RU4(TX_ARGS const uint32_t *);
extern uint32_t _ITM_CALL_CONVENTION _ITM_RaRU4(TX_ARGS const uint32_t *);
extern uint32_t _ITM_CALL_CONVENTION _ITM_RaWU4(TX_ARGS const uint32_t *);
extern uint32_t _ITM_CALL_CONVENTION _ITM_RfWU4(TX_ARGS const uint32_t *);

extern uint64_t _ITM_CALL_CONVENTION _ITM_RU8(TX_ARGS const uint64_t *);
extern uint64_t _ITM_CALL_CONVENTION _ITM_RaRU8(TX_ARGS const uint64_t *);
extern uint64_t _ITM_CALL_CONVENTION _ITM_RaWU8(TX_ARGS const uint64_t *);
extern uint64_t _ITM_CALL_CONVENTION _ITM_RfWU8(TX_ARGS const uint64_t *);

extern float _ITM_CALL_CONVENTION _ITM_RF(TX_ARGS const float *);
extern float _ITM_CALL_CONVENTION _ITM_RaRF(TX_ARGS const float *);
extern float _ITM_CALL_CONVENTION _ITM_RaWF(TX_ARGS const float *);
extern float _ITM_CALL_CONVENTION _ITM_RfWF(TX_ARGS const float *);

extern double _ITM_CALL_CONVENTION _ITM_RD(TX_ARGS const double *);
extern double _ITM_CALL_CONVENTION _ITM_RaRD(TX_ARGS const double *);
extern double _ITM_CALL_CONVENTION _ITM_RaWD(TX_ARGS const double *);
extern double _ITM_CALL_CONVENTION _ITM_RfWD(TX_ARGS const double *);

#ifdef __SSE__
extern __m64 _ITM_CALL_CONVENTION _ITM_RM64(TX_ARGS const __m64 *);
extern __m64 _ITM_CALL_CONVENTION _ITM_RaRM64(TX_ARGS const __m64 *);
extern __m64 _ITM_CALL_CONVENTION _ITM_RaWM64(TX_ARGS const __m64 *);
extern __m64 _ITM_CALL_CONVENTION _ITM_RfWM64(TX_ARGS const __m64 *);

extern __m128 _ITM_CALL_CONVENTION _ITM_RM128(TX_ARGS const __m128 *);
extern __m128 _ITM_CALL_CONVENTION _ITM_RaRM128(TX_ARGS const __m128 *);
extern __m128 _ITM_CALL_CONVENTION _ITM_RaWM128(TX_ARGS const __m128 *);
extern __m128 _ITM_CALL_CONVENTION _ITM_RfWM128(TX_ARGS const __m128 *);
#endif /* __SSE__ */

extern float _Complex _ITM_CALL_CONVENTION _ITM_RCF(TX_ARGS const float _Complex *);
extern float _Complex _ITM_CALL_CONVENTION _ITM_RaRCF(TX_ARGS const float _Complex *);
extern float _Complex _ITM_CALL_CONVENTION _ITM_RaWCF(TX_ARGS const float _Complex *);
extern float _Complex _ITM_CALL_CONVENTION _ITM_RfWCF(TX_ARGS const float _Complex *);

extern double _Complex _ITM_CALL_CONVENTION _ITM_RCD(TX_ARGS const double _Complex *);
extern double _Complex _ITM_CALL_CONVENTION _ITM_RaRCD(TX_ARGS const double _Complex *);
extern double _Complex _ITM_CALL_CONVENTION _ITM_RaWCD(TX_ARGS const double _Complex *);
extern double _Complex _ITM_CALL_CONVENTION _ITM_RfWCD(TX_ARGS const double _Complex *);

extern long double _Complex _ITM_CALL_CONVENTION _ITM_RCE(TX_ARGS const long double _Complex *);
extern long double _Complex _ITM_CALL_CONVENTION _ITM_RaRCE(TX_ARGS const long double _Complex *);
extern long double _Complex _ITM_CALL_CONVENTION _ITM_RaWCE(TX_ARGS const long double _Complex *);
extern long double _Complex _ITM_CALL_CONVENTION _ITM_RfWCE(TX_ARGS const long double _Complex *);


/*** Stores ***/

extern void _ITM_CALL_CONVENTION _ITM_WU1(TX_ARGS const uint8_t *, uint8_t);
extern void _ITM_CALL_CONVENTION _ITM_WaRU1(TX_ARGS const uint8_t *, uint8_t);
extern void _ITM_CALL_CONVENTION _ITM_WaWU1(TX_ARGS const uint8_t *, uint8_t);

extern void _ITM_CALL_CONVENTION _ITM_WU2(TX_ARGS const uint16_t *, uint16_t);
extern void _ITM_CALL_CONVENTION _ITM_WaRU2(TX_ARGS const uint16_t *, uint16_t);
extern void _ITM_CALL_CONVENTION _ITM_WaWU2(TX_ARGS const uint16_t *, uint16_t);

extern void _ITM_CALL_CONVENTION _ITM_WU4(TX_ARGS const uint32_t *, uint32_t);
extern void _ITM_CALL_CONVENTION _ITM_WaRU4(TX_ARGS const uint32_t *, uint32_t);
extern void _ITM_CALL_CONVENTION _ITM_WaWU4(TX_ARGS const uint32_t *, uint32_t);

extern void _ITM_CALL_CONVENTION _ITM_WU8(TX_ARGS const uint64_t *, uint64_t);
extern void _ITM_CALL_CONVENTION _ITM_WaRU8(TX_ARGS const uint64_t *, uint64_t);
extern void _ITM_CALL_CONVENTION _ITM_WaWU8(TX_ARGS const uint64_t *, uint64_t);

extern void _ITM_CALL_CONVENTION _ITM_WF(TX_ARGS const float *, float);
extern void _ITM_CALL_CONVENTION _ITM_WaRF(TX_ARGS const float *, float);
extern void _ITM_CALL_CONVENTION _ITM_WaWF(TX_ARGS const float *, float);

extern void _ITM_CALL_CONVENTION _ITM_WD(TX_ARGS const double *, double);
extern void _ITM_CALL_CONVENTION _ITM_WaRD(TX_ARGS const double *, double);
extern void _ITM_CALL_CONVENTION _ITM_WaWD(TX_ARGS const double *, double);

#ifdef __SSE__
extern void _ITM_CALL_CONVENTION _ITM_WM64(TX_ARGS const __m64 *, __m64);
extern void _ITM_CALL_CONVENTION _ITM_WaRM64(TX_ARGS const __m64 *, __m64);
extern void _ITM_CALL_CONVENTION _ITM_WaWM64(TX_ARGS const __m64 *, __m64);

extern void _ITM_CALL_CONVENTION _ITM_WM128(TX_ARGS const __m128 *, __m128);
extern void _ITM_CALL_CONVENTION _ITM_WaRM128(TX_ARGS const __m128 *, __m128);
extern void _ITM_CALL_CONVENTION _ITM_WaWM128(TX_ARGS const __m128 *, __m128);
#endif /* __SSE__ */

extern void _ITM_CALL_CONVENTION _ITM_WCF(TX_ARGS const float _Complex *, float _Complex);
extern void _ITM_CALL_CONVENTION _ITM_WaRCF(TX_ARGS const float _Complex *, float _Complex);
extern void _ITM_CALL_CONVENTION _ITM_WaWCF(TX_ARGS const float _Complex *, float _Complex);

extern void _ITM_CALL_CONVENTION _ITM_WCD(TX_ARGS const double _Complex *, double _Complex);
extern void _ITM_CALL_CONVENTION _ITM_WaRCD(TX_ARGS const double _Complex *, double _Complex);
extern void _ITM_CALL_CONVENTION _ITM_WaWCD(TX_ARGS const double _Complex *, double _Complex);

extern void _ITM_CALL_CONVENTION _ITM_WCE(TX_ARGS const long double _Complex *, long double _Complex);
extern void _ITM_CALL_CONVENTION _ITM_WaRCE(TX_ARGS const long double _Complex *, long double _Complex);
extern void _ITM_CALL_CONVENTION _ITM_WaWCE(TX_ARGS const long double _Complex *, long double _Complex);


/*** Logging functions ***/

extern void _ITM_CALL_CONVENTION _ITM_LU1(TX_ARGS const uint8_t *);
extern void _ITM_CALL_CONVENTION _ITM_LU2(TX_ARGS const uint16_t *);
extern void _ITM_CALL_CONVENTION _ITM_LU4(TX_ARGS const uint32_t *);
extern void _ITM_CALL_CONVENTION _ITM_LU8(TX_ARGS const uint64_t *);
extern void _ITM_CALL_CONVENTION _ITM_LF(TX_ARGS const float *);
extern void _ITM_CALL_CONVENTION _ITM_LD(TX_ARGS const double *);
extern void _ITM_CALL_CONVENTION _ITM_LE(TX_ARGS const long double *);
extern void _ITM_CALL_CONVENTION _ITM_LM64(TX_ARGS const __m64 *);
extern void _ITM_CALL_CONVENTION _ITM_LM128(TX_ARGS const __m128 *);
extern void _ITM_CALL_CONVENTION _ITM_LCF(TX_ARGS const float _Complex *);
extern void _ITM_CALL_CONVENTION _ITM_LCD(TX_ARGS const double _Complex *);
extern void _ITM_CALL_CONVENTION _ITM_LCE(TX_ARGS const long double _Complex *);
extern void _ITM_CALL_CONVENTION _ITM_LB(TX_ARGS const void *, size_t);


/*** memcpy functions ***/

extern void _ITM_CALL_CONVENTION _ITM_memcpyRnWt(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRnWtaR(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRnWtaW(TX_ARGS void *, const void *, size_t);

extern void _ITM_CALL_CONVENTION _ITM_memcpyRtWn(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtaRWn(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtaWWn(TX_ARGS void *, const void *, size_t);

extern void _ITM_CALL_CONVENTION _ITM_memcpyRtWt(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtWtaR(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtWtaW(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtaRWt(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtaRWtaR(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtaRWtaW(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtaWWt(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtaWWtaR(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtaWWtaW(TX_ARGS void *, const void *, size_t);


/*** memset functions ***/

extern void _ITM_CALL_CONVENTION _ITM_memsetW(TX_ARGS void *, int, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memsetWaR(TX_ARGS void *, int, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memsetWaW(TX_ARGS void *, int, size_t);


/*** memmove functions ***/

extern void _ITM_CALL_CONVENTION _ITM_memmoveRnWt(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRnWtaR(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRnWtaW(TX_ARGS void *, const void *, size_t);

extern void _ITM_CALL_CONVENTION _ITM_memmoveRtWn(TX_ARGS void *, const void *, size_t); 
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtaRWn(TX_ARGS void *, const void *, size_t); 
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtaWWn(TX_ARGS void *, const void *, size_t); 

extern void _ITM_CALL_CONVENTION _ITM_memmoveRtWt(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtWtaR(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtWtaW(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtaRWt(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtaRWtaR(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtaRWtaW(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtaWWt(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtaWWtaR(TX_ARGS void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtaWWtaW(TX_ARGS void *, const void *, size_t);

