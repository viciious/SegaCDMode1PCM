    .text

    .global stereo_test_u8_wav
stereo_test_u8_wav:
    .incbin "data/stereo_test_u8.wav"
stereo_test_u8_wav_end = .

    .global macabre_ima_wav
macabre_ima_wav:
    .incbin "data/macabre_ima.wav"
macabre_ima_wav_end = .

    .global macabre_sb4_wav
macabre_sb4_wav:
    .incbin "data/macabre_sb4.wav"
macabre_sb4_wav_end = .

    .align  4

stereo_test_u8_wav_len:
    .global stereo_test_u8_wav_len
.long stereo_test_u8_wav_end-stereo_test_u8_wav

macabre_ima_wav_len:
    .global macabre_ima_wav_len
.long macabre_ima_wav_end-macabre_ima_wav

macabre_sb4_wav_len:
    .global macabre_sb4_wav_len
.long macabre_sb4_wav_end-macabre_sb4_wav
