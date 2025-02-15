#include "princeton.h"

#include "../blocks/const.h"
#include "../blocks/decoder.h"
#include "../blocks/encoder.h"
#include "../blocks/generic.h"
#include "../blocks/math.h"

#include "../blocks/custom_btn_i.h"

/*
 * Help
 * https://phreakerclub.com/447
 *
 */

#define TAG "SubGhzProtocolPrinceton"

#define PRINCETON_GUARD_TIME_DEFALUT 30 //GUARD_TIME = PRINCETON_GUARD_TIME_DEFALUT * TE
// Guard Time value should be between 15 -> 72 otherwise default value will be used

static const SubGhzBlockConst subghz_protocol_princeton_const = {
    .te_short = 390,
    .te_long = 1170,
    .te_delta = 300,
    .min_count_bit_for_found = 24,
};

struct SubGhzProtocolDecoderPrinceton {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    uint32_t te;
    uint32_t last_data;
    uint32_t guard_time;
};

struct SubGhzProtocolEncoderPrinceton {
    SubGhzProtocolEncoderBase base;

    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;

    uint32_t te;
    uint32_t guard_time;
};

typedef enum {
    PrincetonDecoderStepReset = 0,
    PrincetonDecoderStepSaveDuration,
    PrincetonDecoderStepCheckDuration,
} PrincetonDecoderStep;

const SubGhzProtocolDecoder subghz_protocol_princeton_decoder = {
    .alloc = subghz_protocol_decoder_princeton_alloc,
    .free = subghz_protocol_decoder_princeton_free,

    .feed = subghz_protocol_decoder_princeton_feed,
    .reset = subghz_protocol_decoder_princeton_reset,

    .get_hash_data = subghz_protocol_decoder_princeton_get_hash_data,
    .serialize = subghz_protocol_decoder_princeton_serialize,
    .deserialize = subghz_protocol_decoder_princeton_deserialize,
    .get_string = subghz_protocol_decoder_princeton_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_princeton_encoder = {
    .alloc = subghz_protocol_encoder_princeton_alloc,
    .free = subghz_protocol_encoder_princeton_free,

    .deserialize = subghz_protocol_encoder_princeton_deserialize,
    .stop = subghz_protocol_encoder_princeton_stop,
    .yield = subghz_protocol_encoder_princeton_yield,
};

const SubGhzProtocol subghz_protocol_princeton = {
    .name = SUBGHZ_PROTOCOL_PRINCETON_NAME,
    .type = SubGhzProtocolTypeStatic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_868 | SubGhzProtocolFlag_315 |
            SubGhzProtocolFlag_AM | SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load |
            SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Send | SubGhzProtocolFlag_Princeton,

    .decoder = &subghz_protocol_princeton_decoder,
    .encoder = &subghz_protocol_princeton_encoder,
};

void* subghz_protocol_encoder_princeton_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolEncoderPrinceton* instance = malloc(sizeof(SubGhzProtocolEncoderPrinceton));

    instance->base.protocol = &subghz_protocol_princeton;
    instance->generic.protocol_name = instance->base.protocol->name;

    instance->encoder.repeat = 10;
    instance->encoder.size_upload = 52; //max 24bit*2 + 2 (start, stop)
    instance->encoder.upload = malloc(instance->encoder.size_upload * sizeof(LevelDuration));
    instance->encoder.is_running = false;
    return instance;
}

void subghz_protocol_encoder_princeton_free(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderPrinceton* instance = context;
    free(instance->encoder.upload);
    free(instance);
}

// Get custom button code
static uint8_t subghz_protocol_princeton_get_btn_code(void) {
    uint8_t custom_btn_id = subghz_custom_btn_get();
    uint8_t original_btn_code = subghz_custom_btn_get_original();
    uint8_t btn = original_btn_code;

    // Set custom button
    if((custom_btn_id == SUBGHZ_CUSTOM_BTN_OK) && (original_btn_code != 0)) {
        // Restore original button code
        btn = original_btn_code;
    } else if(custom_btn_id == SUBGHZ_CUSTOM_BTN_UP) {
        switch(original_btn_code) {
        case 0x1:
            btn = 0x2;
            break;
        case 0x2:
            btn = 0x1;
            break;
        case 0x4:
            btn = 0x2;
            break;
        case 0x8:
            btn = 0x2;
            break;
        case 0xF:
            btn = 0x2;
            break;
        // Second encoding type
        case 0x30:
            btn = 0xC0;
            break;
        case 0xC0:
            btn = 0x30;
            break;
        case 0x03:
            btn = 0xC0;
            break;
        case 0x0C:
            btn = 0xC0;
            break;

        default:
            break;
        }
    } else if(custom_btn_id == SUBGHZ_CUSTOM_BTN_DOWN) {
        switch(original_btn_code) {
        case 0x1:
            btn = 0x4;
            break;
        case 0x2:
            btn = 0x4;
            break;
        case 0x4:
            btn = 0x1;
            break;
        case 0x8:
            btn = 0x1;
            break;
        case 0xF:
            btn = 0x1;
            break;
        // Second encoding type
        case 0x30:
            btn = 0x03;
            break;
        case 0xC0:
            btn = 0x03;
            break;
        case 0x03:
            btn = 0x30;
            break;
        case 0x0C:
            btn = 0x03;
            break;

        default:
            break;
        }
    } else if(custom_btn_id == SUBGHZ_CUSTOM_BTN_LEFT) {
        switch(original_btn_code) {
        case 0x1:
            btn = 0x8;
            break;
        case 0x2:
            btn = 0x8;
            break;
        case 0x4:
            btn = 0x8;
            break;
        case 0x8:
            btn = 0x4;
            break;
        case 0xF:
            btn = 0x4;
            break;
        // Second encoding type
        case 0x30:
            btn = 0x0C;
            break;
        case 0xC0:
            btn = 0x0C;
            break;
        case 0x03:
            btn = 0x0C;
            break;
        case 0x0C:
            btn = 0x30;
            break;

        default:
            break;
        }
    } else if(custom_btn_id == SUBGHZ_CUSTOM_BTN_RIGHT) {
        switch(original_btn_code) {
        case 0x1:
            btn = 0xF;
            break;
        case 0x2:
            btn = 0xF;
            break;
        case 0x4:
            btn = 0xF;
            break;
        case 0x8:
            btn = 0xF;
            break;
        case 0xF:
            btn = 0x8;
            break;

        default:
            break;
        }
    }

    return btn;
}

/**
 * Generating an upload from data.
 * @param instance Pointer to a SubGhzProtocolEncoderPrinceton instance
 * @return true On success
 */
static bool
    subghz_protocol_encoder_princeton_get_upload(SubGhzProtocolEncoderPrinceton* instance) {
    furi_assert(instance);

    // Generate new key using custom or default button
    instance->generic.btn = subghz_protocol_princeton_get_btn_code();

    // Reconstruction of the data
    // If we have 8bit button code move serial to left by 8 bits (and 4 if 4 bits)
    if(instance->generic.btn == 0x30 || instance->generic.btn == 0xC0 ||
       instance->generic.btn == 0x03 || instance->generic.btn == 0x0C) {
        instance->generic.data =
            ((uint64_t)instance->generic.serial << 8 | (uint64_t)instance->generic.btn);
    } else {
        instance->generic.data =
            ((uint64_t)instance->generic.serial << 4 | (uint64_t)instance->generic.btn);
    }

    size_t index = 0;
    size_t size_upload = (instance->generic.data_count_bit * 2) + 2;
    if(size_upload > instance->encoder.size_upload) {
        FURI_LOG_E(TAG, "Size upload exceeds allocated encoder buffer.");
        return false;
    } else {
        instance->encoder.size_upload = size_upload;
    }

    //Send key data
    for(uint8_t i = instance->generic.data_count_bit; i > 0; i--) {
        if(bit_read(instance->generic.data, i - 1)) {
            //send bit 1
            instance->encoder.upload[index++] =
                level_duration_make(true, (uint32_t)instance->te * 3);
            instance->encoder.upload[index++] = level_duration_make(false, (uint32_t)instance->te);
        } else {
            //send bit 0
            instance->encoder.upload[index++] = level_duration_make(true, (uint32_t)instance->te);
            instance->encoder.upload[index++] =
                level_duration_make(false, (uint32_t)instance->te * 3);
        }
    }

    //Send Stop bit
    instance->encoder.upload[index++] = level_duration_make(true, (uint32_t)instance->te);
    //Send PT_GUARD_TIME
    instance->encoder.upload[index++] =
        level_duration_make(false, (uint32_t)instance->te * instance->guard_time);

    return true;
}

/** 
 * Analysis of received data
 * @param instance Pointer to a SubGhzBlockGeneric* instance
 */
static void subghz_protocol_princeton_check_remote_controller(SubGhzBlockGeneric* instance) {
    // Parse button modes for second encoding type (and serial is smaller)
    // Button code is 8bit and has fixed values of one of these
    // Exclude button code for each type from serial number before parsing
    if((instance->data & 0xFF) == 0x30 || (instance->data & 0xFF) == 0xC0 ||
       (instance->data & 0xFF) == 0x03 || (instance->data & 0xFF) == 0x0C) {
        instance->serial = instance->data >> 8;
        instance->btn = instance->data & 0xFF;
    } else {
        instance->serial = instance->data >> 4;
        instance->btn = instance->data & 0xF;
    }

    // Save original button for later use
    if(subghz_custom_btn_get_original() == 0) {
        subghz_custom_btn_set_original(instance->btn);
    }
    subghz_custom_btn_set_max(4);
}

SubGhzProtocolStatus
    subghz_protocol_encoder_princeton_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolEncoderPrinceton* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;
    do {
        ret = subghz_block_generic_deserialize_check_count_bit(
            &instance->generic,
            flipper_format,
            subghz_protocol_princeton_const.min_count_bit_for_found);
        if(ret != SubGhzProtocolStatusOk) {
            break;
        }
        if(!flipper_format_rewind(flipper_format)) {
            FURI_LOG_E(TAG, "Rewind error");
            ret = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        if(!flipper_format_read_uint32(flipper_format, "TE", (uint32_t*)&instance->te, 1)) {
            FURI_LOG_E(TAG, "Missing TE");
            ret = SubGhzProtocolStatusErrorParserTe;
            break;
        }
        //optional parameter parameter
        if(!flipper_format_read_uint32(
               flipper_format, "Guard_time", (uint32_t*)&instance->guard_time, 1)) {
            instance->guard_time = PRINCETON_GUARD_TIME_DEFALUT;
        } else {
            // Guard Time value should be between 15 -> 72 otherwise default value will be used
            if((instance->guard_time < 15) || (instance->guard_time > 72)) {
                instance->guard_time = PRINCETON_GUARD_TIME_DEFALUT;
            }
        }

        flipper_format_read_uint32(
            flipper_format, "Repeat", (uint32_t*)&instance->encoder.repeat, 1);

        // Get button and serial before calling get_upload
        subghz_protocol_princeton_check_remote_controller(&instance->generic);

        if(!subghz_protocol_encoder_princeton_get_upload(instance)) {
            ret = SubGhzProtocolStatusErrorEncoderGetUpload;
            break;
        }

        if(!flipper_format_rewind(flipper_format)) {
            FURI_LOG_E(TAG, "Rewind error");
            break;
        }
        uint8_t key_data[sizeof(uint64_t)] = {0};
        for(size_t i = 0; i < sizeof(uint64_t); i++) {
            key_data[sizeof(uint64_t) - i - 1] = (instance->generic.data >> i * 8) & 0xFF;
        }
        if(!flipper_format_update_hex(flipper_format, "Key", key_data, sizeof(uint64_t))) {
            FURI_LOG_E(TAG, "Unable to add Key");
            break;
        }
        instance->encoder.is_running = true;
    } while(false);

    return ret;
}

void subghz_protocol_encoder_princeton_stop(void* context) {
    SubGhzProtocolEncoderPrinceton* instance = context;
    instance->encoder.is_running = false;
}

LevelDuration subghz_protocol_encoder_princeton_yield(void* context) {
    SubGhzProtocolEncoderPrinceton* instance = context;

    if(instance->encoder.repeat == 0 || !instance->encoder.is_running) {
        instance->encoder.is_running = false;
        return level_duration_reset();
    }

    LevelDuration ret = instance->encoder.upload[instance->encoder.front];

    if(++instance->encoder.front == instance->encoder.size_upload) {
        instance->encoder.repeat--;
        instance->encoder.front = 0;
    }

    return ret;
}

void* subghz_protocol_decoder_princeton_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderPrinceton* instance = malloc(sizeof(SubGhzProtocolDecoderPrinceton));
    instance->base.protocol = &subghz_protocol_princeton;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void subghz_protocol_decoder_princeton_free(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderPrinceton* instance = context;
    free(instance);
}

void subghz_protocol_decoder_princeton_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderPrinceton* instance = context;
    instance->decoder.parser_step = PrincetonDecoderStepReset;
    instance->last_data = 0;
}

void subghz_protocol_decoder_princeton_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderPrinceton* instance = context;

    switch(instance->decoder.parser_step) {
    case PrincetonDecoderStepReset:
        if((!level) && (DURATION_DIFF(duration, subghz_protocol_princeton_const.te_short * 36) <
                        subghz_protocol_princeton_const.te_delta * 36)) {
            //Found Preambula
            instance->decoder.parser_step = PrincetonDecoderStepSaveDuration;
            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
            instance->te = 0;
            instance->guard_time = PRINCETON_GUARD_TIME_DEFALUT;
        }
        break;
    case PrincetonDecoderStepSaveDuration:
        //save duration
        if(level) {
            instance->decoder.te_last = duration;
            instance->te += duration;
            instance->decoder.parser_step = PrincetonDecoderStepCheckDuration;
        }
        break;
    case PrincetonDecoderStepCheckDuration:
        if(!level) {
            if(duration >= ((uint32_t)subghz_protocol_princeton_const.te_long * 2)) {
                instance->decoder.parser_step = PrincetonDecoderStepSaveDuration;
                if(instance->decoder.decode_count_bit ==
                   subghz_protocol_princeton_const.min_count_bit_for_found) {
                    if((instance->last_data == instance->decoder.decode_data) &&
                       instance->last_data) {
                        instance->te /= (instance->decoder.decode_count_bit * 4 + 1);

                        instance->generic.data = instance->decoder.decode_data;
                        instance->generic.data_count_bit = instance->decoder.decode_count_bit;
                        instance->guard_time = roundf((float)duration / instance->te);
                        // Guard Time value should be between 15 -> 72 otherwise default value will be used
                        if((instance->guard_time < 15) || (instance->guard_time > 72)) {
                            instance->guard_time = PRINCETON_GUARD_TIME_DEFALUT;
                        }

                        if(instance->base.callback)
                            instance->base.callback(&instance->base, instance->base.context);
                    }
                    instance->last_data = instance->decoder.decode_data;
                }
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                instance->te = 0;
                break;
            }

            instance->te += duration;

            if((DURATION_DIFF(instance->decoder.te_last, subghz_protocol_princeton_const.te_short) <
                subghz_protocol_princeton_const.te_delta) &&
               (DURATION_DIFF(duration, subghz_protocol_princeton_const.te_long) <
                subghz_protocol_princeton_const.te_delta * 3)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                instance->decoder.parser_step = PrincetonDecoderStepSaveDuration;
            } else if(
                (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_princeton_const.te_long) <
                 subghz_protocol_princeton_const.te_delta * 3) &&
                (DURATION_DIFF(duration, subghz_protocol_princeton_const.te_short) <
                 subghz_protocol_princeton_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->decoder.parser_step = PrincetonDecoderStepSaveDuration;
            } else {
                instance->decoder.parser_step = PrincetonDecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = PrincetonDecoderStepReset;
        }
        break;
    }
}

uint8_t subghz_protocol_decoder_princeton_get_hash_data(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderPrinceton* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus subghz_protocol_decoder_princeton_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolDecoderPrinceton* instance = context;
    SubGhzProtocolStatus ret =
        subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
    if((ret == SubGhzProtocolStatusOk) &&
       !flipper_format_write_uint32(flipper_format, "TE", &instance->te, 1)) {
        FURI_LOG_E(TAG, "Unable to add TE");
        ret = SubGhzProtocolStatusErrorParserTe;
    }
    if((ret == SubGhzProtocolStatusOk) &&
       !flipper_format_write_uint32(flipper_format, "Guard_time", &instance->guard_time, 1)) {
        FURI_LOG_E(TAG, "Unable to add Guard_time");
        ret = SubGhzProtocolStatusErrorParserOthers;
    }

    return ret;
}

SubGhzProtocolStatus
    subghz_protocol_decoder_princeton_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolDecoderPrinceton* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;
    do {
        ret = subghz_block_generic_deserialize_check_count_bit(
            &instance->generic,
            flipper_format,
            subghz_protocol_princeton_const.min_count_bit_for_found);
        if(ret != SubGhzProtocolStatusOk) {
            break;
        }
        if(!flipper_format_rewind(flipper_format)) {
            FURI_LOG_E(TAG, "Rewind error");
            ret = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        if(!flipper_format_read_uint32(flipper_format, "TE", (uint32_t*)&instance->te, 1)) {
            FURI_LOG_E(TAG, "Missing TE");
            ret = SubGhzProtocolStatusErrorParserTe;
            break;
        }
        if(!flipper_format_read_uint32(
               flipper_format, "Guard_time", (uint32_t*)&instance->guard_time, 1)) {
            instance->guard_time = PRINCETON_GUARD_TIME_DEFALUT;
        } else {
            // Guard Time value should be between 15 -> 72 otherwise default value will be used
            if((instance->guard_time < 15) || (instance->guard_time > 72)) {
                instance->guard_time = PRINCETON_GUARD_TIME_DEFALUT;
            }
        }

    } while(false);

    return ret;
}

void subghz_protocol_decoder_princeton_get_string(void* context, FuriString* output) {
    furi_assert(context);
    SubGhzProtocolDecoderPrinceton* instance = context;
    subghz_protocol_princeton_check_remote_controller(&instance->generic);
    uint32_t data_rev = subghz_protocol_blocks_reverse_key(
        instance->generic.data, instance->generic.data_count_bit);

    if(instance->generic.btn == 0x30 || instance->generic.btn == 0xC0 ||
       instance->generic.btn == 0x03 || instance->generic.btn == 0x0C) {
        furi_string_cat_printf(
            output,
            "%s %dbit\r\n"
            "Key:0x%08lX\r\n"
            "Yek:0x%08lX\r\n"
            "Sn:0x%05lX Btn:%02X (8b)\r\n"
            "Te:%luus  GT:Te*%lu\r\n",
            instance->generic.protocol_name,
            instance->generic.data_count_bit,
            (uint32_t)(instance->generic.data & 0xFFFFFF),
            data_rev,
            instance->generic.serial,
            instance->generic.btn,
            instance->te,
            instance->guard_time);
    } else {
        furi_string_cat_printf(
            output,
            "%s %dbit\r\n"
            "Key:0x%08lX\r\n"
            "Yek:0x%08lX\r\n"
            "Sn:0x%05lX Btn:%01X (4b)\r\n"
            "Te:%luus  GT:Te*%lu\r\n",
            instance->generic.protocol_name,
            instance->generic.data_count_bit,
            (uint32_t)(instance->generic.data & 0xFFFFFF),
            data_rev,
            instance->generic.serial,
            instance->generic.btn,
            instance->te,
            instance->guard_time);
    }
}
