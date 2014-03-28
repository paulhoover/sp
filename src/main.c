#include "pebble.h"
#include "xprintf.h"

enum {
	SLIDE_ID = 0x0,
	SLIDE_NEXT = 0x1,
	SLIDE_PREV = 0x2,
	SLIDE_PRESENTATION_TITLE = 0x3,
	SLIDE_COUNT = 0x4,
	SLIDE_NOTES = 0x5,
	SLIDE_PRESENTATION_START = 0x6,
	SLIDE_START = 0x7,
	SLIDE_IS_CONFIGURED = 0x8
};


static Window *timer_window;

static TextLayer *slide_number_layer;
static TextLayer *slide_time_layer;
static TextLayer *presentation_time_layer;
static TextLayer *setup_layer;


static AppSync sync;
static uint8_t sync_buffer[2000];


static char slideDurationStr[10] = "";
static char presentationDurationStr[10] = "";
static char slideCountStr[5] = "";
static char slideNumberStr[5] = "";
static char progressStr[15] = "";
static int slideStartTime = 0;
static int presentationStartTime = 0;

static bool presentationLoaded = false;
static bool isConfigured = false;

static void show_setup_text(const char* message) {
	APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "show_setup_text");
	APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, message);
	Layer *window_layer = window_get_root_layer(timer_window);
	text_layer_set_text(setup_layer, message);
	layer_add_child(window_layer, text_layer_get_layer(setup_layer));	
}

static void update_status_text(bool bluetoothStatus) {

	if(!bluetoothStatus) {
		show_setup_text("\nBluetooth Connection Required");
	} else if(!isConfigured) {
		show_setup_text("Configure SlideConductr from the Pebble app on your phone");
	} else if(!presentationLoaded) {
		show_setup_text("Connecting...");
	} else {
		layer_remove_from_parent(text_layer_get_layer(setup_layer));			
	}
}

void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
	
	switch (key) {
	case SLIDE_COUNT:
		strcpy(slideCountStr, new_tuple->value->cstring);
//		APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "sync_tuple_changed_callback:slide_count %s", slideCountStr);
		break;

	case SLIDE_ID:
		strcpy(slideNumberStr, new_tuple->value->cstring);
//		APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "sync_tuple_changed_callback:slide_number %s", slideNumberStr);

		break;
	case SLIDE_PRESENTATION_TITLE:
		if( strcmp(new_tuple->value->cstring, "") != 0) {
			presentationLoaded = true;
			update_status_text(bluetooth_connection_service_peek());
		}
		break;
	//case SLIDE_NOTES:
	//	text_layer_set_text(slide_time_layer, new_tuple->value->cstring);
	//	break;
	case SLIDE_START:
		slideStartTime = new_tuple->value->int32;
//		APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "sync_tuple_changed_callback:SLIDE_START %lu", new_tuple->value->int32);

		break;
	case SLIDE_PRESENTATION_START:
		presentationStartTime = new_tuple->value->int32;
		break;
	case SLIDE_IS_CONFIGURED:
		if( strcmp(new_tuple->value->cstring, "") != 0) {
			isConfigured = true;
			update_status_text(bluetooth_connection_service_peek());
		}
		break;
	}
	if (key == SLIDE_COUNT || key == SLIDE_ID) {
		xsprintf(progressStr, "Slide %s / %s\0", slideNumberStr, slideCountStr);
		APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "c: %s", progressStr);
		text_layer_set_text(slide_number_layer, progressStr);
	}
	APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "sync_tuple_changed_callback4");
}


static void tick_handler(struct tm *tick_time, TimeUnits units_changed)
{
	int now = time(NULL);
	int slideDuration = now - slideStartTime;
	int presentationDuration = now - presentationStartTime;


	if(slideDuration <= 0) { slideDuration = 0; }
	if(presentationStartTime <= 0) { presentationDuration = 0; }
	
	xsprintf(slideDurationStr, "%02d:%02d", (int)(slideDuration / 60), slideDuration % 60);
	xsprintf(presentationDurationStr, "%02d:%02d", (int)(presentationDuration / 60), presentationDuration % 60);

	APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "s: %s", slideDurationStr);
	APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "p: %s", presentationDurationStr);
	
	//Change the TextLayers to show the new time!
	text_layer_set_text(slide_time_layer, slideDurationStr);
	text_layer_set_text(presentation_time_layer, presentationDurationStr);
	
	APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "tick_handler7");

}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "up_click_handler");

	if (!presentationLoaded) return; // don't handle button clicks if they happen before a presentation is loaded

	Tuplet next_tuple = TupletCString(SLIDE_NEXT, ".");

	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	if (iter == NULL) {
		return;
	}

	dict_write_tuplet(iter, &next_tuple);
	dict_write_end(iter);

	app_message_outbox_send();
}
static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "down_click_handler");

	if (!presentationLoaded) return;

	Tuplet prev_tuple = TupletCString(SLIDE_PREV, ".");

	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	if (iter == NULL) {
		return;
	}

	dict_write_tuplet(iter, &prev_tuple);
	dict_write_end(iter);

	app_message_outbox_send();
}

static void click_config_provider(void *context) {
	window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
	window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}


static void window_load(Window *window) {
	Tuplet initial_values[] = {
		TupletCString(SLIDE_ID, ""),
		TupletCString(SLIDE_NEXT, ""),
		TupletCString(SLIDE_PREV, ""),
		TupletCString(SLIDE_PRESENTATION_TITLE, ""),
		TupletCString(SLIDE_COUNT, ""),
		TupletCString(SLIDE_NOTES, ""),
		TupletInteger(SLIDE_PRESENTATION_START, (int32_t)0),
		TupletInteger(SLIDE_START, (int32_t)0),
		TupletCString(SLIDE_IS_CONFIGURED, ""),
	};
	app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
		sync_tuple_changed_callback, NULL, NULL);
}

static void init(void) {
	timer_window = window_create();

	window_set_click_config_provider(timer_window, click_config_provider);
	window_set_window_handlers(timer_window, (WindowHandlers) {
		.load = window_load,
			//    .unload = window_unload
	});

	tick_timer_service_subscribe(SECOND_UNIT, (TickHandler)tick_handler);

	const int inbound_size = 640;
	const int outbound_size = 64;
	app_message_open(inbound_size, outbound_size);

	window_stack_push(timer_window, true /* Animated */);

	Layer *window_layer = window_get_root_layer(timer_window);
	GRect bounds = layer_get_frame(window_layer);

	slide_number_layer = text_layer_create(GRect(0, 0, bounds.size.w /* width */, 30 /* height */));
	text_layer_set_font(slide_number_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
	text_layer_set_text_alignment(slide_number_layer, GTextAlignmentCenter);
	layer_add_child(window_layer, text_layer_get_layer(slide_number_layer));

	slide_time_layer = text_layer_create(GRect(0, 30, bounds.size.w /* width */, 60 /* height */));
	text_layer_set_font(slide_time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
	text_layer_set_text_alignment(slide_time_layer, GTextAlignmentCenter);
	layer_add_child(window_layer, text_layer_get_layer(slide_time_layer));

	presentation_time_layer = text_layer_create(GRect(0, 90, bounds.size.w /* width */, 64 /* height */));
	text_layer_set_font(presentation_time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
	text_layer_set_text_alignment(presentation_time_layer, GTextAlignmentCenter);
	text_layer_set_background_color(presentation_time_layer, GColorBlack);
	text_layer_set_text_color(presentation_time_layer, GColorWhite);
	layer_add_child(window_layer, text_layer_get_layer(presentation_time_layer));

	setup_layer = text_layer_create(GRect(0, 0, bounds.size.w /* width */, bounds.size.h /* height */));
	text_layer_set_font(setup_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
	text_layer_set_text_alignment(setup_layer, GTextAlignmentCenter);
	text_layer_set_background_color(setup_layer, GColorWhite);
	text_layer_set_text_color(setup_layer, GColorBlack);
	
	update_status_text(bluetooth_connection_service_peek());
	bluetooth_connection_service_subscribe((BluetoothConnectionHandler)update_status_text);
}


static void deinit(void) {
	tick_timer_service_unsubscribe();
	text_layer_destroy(slide_number_layer);
	text_layer_destroy(slide_time_layer);
	text_layer_destroy(presentation_time_layer);
	window_destroy(timer_window);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
