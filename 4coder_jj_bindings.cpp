/* Original Author: Armand (JJ) Karambasis */
#include "4coder_default_include.cpp"
#include "4coder_default_map.cpp"
#include "4coder_jj.config"
#include <string.h>

#define COLOR_LEXER_HASH_ENTRY_COUNT 409600

struct lexer_hash_entry
{
    String_Const_u8 value;
    Code_Index_Note_Kind note_kind;
};

struct lexer_hash_table
{
    System_Mutex mutex;
    Arena arena;
    lexer_hash_entry* entries;    
};

enum Language_Type
{
    LANGUAGE_TYPE_UNKNOWN,
    LANGUAGE_TYPE_CPP
};

lexer_hash_table init_lexer_hash_table()
{
    lexer_hash_table result;
    result.arena = make_arena_system(MB(2));
    result.mutex = system_mutex_make();    
    result.entries = push_array_zero(&result.arena, lexer_hash_entry, COLOR_LEXER_HASH_ENTRY_COUNT);
    for(u32 lexer_index = 0; lexer_index < COLOR_LEXER_HASH_ENTRY_COUNT; lexer_index++)
    {
        result.entries[lexer_index].value = {};
        result.entries[lexer_index].note_kind = (Code_Index_Note_Kind)-1;
    }    
    return result;
}

global lexer_hash_table global_lexer_hash_table;

u32 lexer_hash_index(String_Const_u8 string)
{
    u32 result = 0;
    u32 rand1 = 31414;
    u32 rand2 = 27183;
    
    u8* key = string.str;    
    while(string.size)
    {
        result *= rand1;
        result += *key;
        rand1 *= rand2;
        key++;
        string.size--;
    }
    
    result = result % COLOR_LEXER_HASH_ENTRY_COUNT;
    return result;
}

b32 has_lexer_entry(u32 hash)
{    
    lexer_hash_entry* result = global_lexer_hash_table.entries + hash;
    if(result->note_kind == (Code_Index_Note_Kind)-1)
        return false; 
    return true;
}

u32 lexer_hash(String_Const_u8 string)
{
    u32 hash = lexer_hash_index(string);            
    if(has_lexer_entry(hash))
    {
        lexer_hash_entry* entry = global_lexer_hash_table.entries + hash;    
        while((entry->note_kind != (Code_Index_Note_Kind)-1) && !string_match(entry->value, string))
        {         
            hash++;        
            entry = global_lexer_hash_table.entries + hash;
        }
    }    
    return hash;
}

void add_color_lexer_entry(String_Const_u8 value, Code_Index_Note_Kind note_kind)
{        
    u32 hash = lexer_hash(value);               
    b32 make_new_entry = !has_lexer_entry(hash);
    if(!make_new_entry)    
        make_new_entry = !string_match(global_lexer_hash_table.entries[hash].value, value);
    
    if(make_new_entry)
    {        
        global_lexer_hash_table.entries[hash].value = push_string_copy(&global_lexer_hash_table.arena, value);            
        global_lexer_hash_table.entries[hash].note_kind = note_kind;        
    }
    
    if(global_lexer_hash_table.entries[hash].note_kind != note_kind)
        global_lexer_hash_table.entries[hash].note_kind = note_kind;
}

lexer_hash_entry* get_color_lexer_entry(String_Const_u8 value)
{
    u32 hash = lexer_hash(value);
    if(has_lexer_entry(hash))
        return &global_lexer_hash_table.entries[hash];        
    return NULL;        
}

inline FColor make_color(u32 Color)
{
    FColor Result;
    Result.argb  = Color;
    return Result;
}

void view_settings(Application_Links* app, View_ID view)
{
    view_set_setting(app, view, ViewSetting_ShowScrollbar, false);
    view_set_setting(app, view, ViewSetting_ShowFileBar, true);
}

global b32 global_macro_recording;
CUSTOM_COMMAND_SIG(toggle_macro)
{
    if(global_macro_recording)
    {
        keyboard_macro_finish_recording(app);
        global_macro_recording = false;                
    }
    else
    {        
        keyboard_macro_start_recording(app);
        global_macro_recording = true;
    }
}

CUSTOM_COMMAND_SIG(replay_macro)
{
    if(!global_macro_recording)
    {
        keyboard_macro_replay(app);
    }
}

void list_all_locations(Application_Links* app, List_All_Locations_Flag flags)
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    if(buffer != 0)
    {
        list_all_locations__generic_query(app, flags);
        view = get_active_view(app, Access_Always);
        buffer = view_get_buffer(app, view, Access_Always);
        Marker_List *list = get_or_make_list_for_buffer(app, &global_heap, buffer);
        if (list != 0)
        {
            Jump_Lister_Result jump = get_jump_index_from_user(app, list,
                                                               "Function:");
            jump_to_jump_lister_result(app, view, list, &jump);
        }
    }
}

void list_all_locations_selection(Application_Links* app, List_All_Locations_Flag flags)
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    if(buffer != 0)
    {
        list_all_locations__generic_view_range(app, flags);
        view = get_active_view(app, Access_Always);
        buffer = view_get_buffer(app, view, Access_Always);
        Marker_List *list = get_or_make_list_for_buffer(app, &global_heap, buffer);
        if (list != 0)
        {
            Jump_Lister_Result jump = get_jump_index_from_user(app, list,
                                                               "Function:");
            jump_to_jump_lister_result(app, view, list, &jump);
        }
    }
}

CUSTOM_COMMAND_SIG(jj_kill_to_end_of_line)
{
    View_ID view = get_active_view(app, Access_Read);            
    i64 pos0 = view_get_cursor_pos(app, view);
    seek_end_of_line(app);        
    i64 pos1 = view_get_cursor_pos(app, view);            
    if(pos0 == pos1)
        pos1++;
    
    Range_i64 range;
    range.min = pos0;
    range.max = pos1;
    
    Buffer_ID buffer = view_get_buffer(app, view, Access_Write);
    buffer_replace_range(app, buffer, range, string_u8_empty);
    auto_indent_range(app);
}

global b32 global_mark;
CUSTOM_COMMAND_SIG(jj_set_mark)
{    
    if(!global_mark)
        set_mark(app);           
    global_mark = !global_mark;
}

CUSTOM_COMMAND_SIG(jj_copy)
{
    if(global_mark)
    {
        copy(app);
        global_mark = !global_mark;
    }    
}

CUSTOM_COMMAND_SIG(jj_cut)
{
    if(global_mark)
    {
        cut(app);
        global_mark = !global_mark;
    }
}

CUSTOM_COMMAND_SIG(list_all_locations_lister_substring)
{   
    if(global_mark)
    {
        list_all_locations_selection(app, ListAllLocationsFlag_MatchSubstring);
        global_mark = !global_mark;
    }
    else    
        list_all_locations(app, ListAllLocationsFlag_MatchSubstring);
}

CUSTOM_COMMAND_SIG(list_all_locations_lister)
{
    if(global_mark)
    {
        list_all_locations_selection(app, 0);
        global_mark = !global_mark;
    }    
    else
        list_all_locations(app, 0);
}

function void
draw_mark_range(Application_Links* app, View_ID view, Text_Layout_ID text_layout_id, f32 roundness)
{
    View_ID active_view = get_active_view(app, Access_Always);        
    b32 is_active_view = active_view == view;
    if(global_mark && is_active_view)
    {            
        i64 cursor_pos = view_get_cursor_pos(app, view);
        i64 mark_pos = view_get_mark_pos(app, view);
        
        Range_i64 range;                
        range.min = (cursor_pos < mark_pos) ? cursor_pos : mark_pos;
        range.max = (cursor_pos > mark_pos) ? cursor_pos : mark_pos;
        
        draw_character_block(app, text_layout_id, range, roundness, make_color(RANGE_HIGHLIGHT_COLOR));
    }
}

function void
draw_cpp_tokens(Application_Links* app, Buffer_ID buffer, Text_Layout_ID text_layout_id)
{
    Token_Array token_array = get_token_array_from_buffer(app, buffer);
    Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
    if (token_array.tokens != 0)
    {
        Scratch_Block scratch(app);        

        i64 first_index = token_index_from_pos(&token_array, visible_range.first);
        Token_Iterator_Array iterator = token_iterator_index(0, &token_array, first_index);
        
        Comment_Highlight_Pair pairs[] = 
        {
            {string_u8_litexpr("NOTE"), COMMENT_NOTE},
            {string_u8_litexpr("TODO"), COMMENT_TODO},
            {string_u8_litexpr("IMPORTANT"), COMMENT_IMPORTANT}
        };
        
        for(;;)
        {
            Temp_Memory_Block temp(scratch);
            Token* token = token_it_read(&iterator);
            if(token->pos >= visible_range.one_past_last)
                break;           
            
            ARGB_Color color = FOREGROUND_COLOR;
            switch(token->kind)
            {
                case TokenBaseKind_Keyword:
                {
                    color = TYPE_COLOR;
                } break;
                
                case TokenBaseKind_Comment:
                {
                    color = COMMENT_COLOR;
                } break;
                
                case TokenBaseKind_Identifier:
                {
                    String_Const_u8 identifier = push_token_lexeme(app, scratch, buffer, token);                                        
                    lexer_hash_entry* entry = get_color_lexer_entry(identifier);                                        
                    if(entry)
                    {
                        if(entry->note_kind == CodeIndexNote_Type) color = TYPE_COLOR;
                        else if(entry->note_kind == CodeIndexNote_Function) color = FUNCTION_COLOR;
                        else if(entry->note_kind == CodeIndexNote_Macro) color = MACRO_COLOR;
                    }                    
                } break;
                
                                
                case TokenBaseKind_LiteralString:
                case TokenBaseKind_LiteralInteger:
                case TokenBaseKind_LiteralFloat:
                {
                    color = DATA_COLOR;
                } break;
                
                default:
                {
                    switch(token->sub_kind)
                    {
                        case TokenCppKind_LiteralTrue:
                        case TokenCppKind_LiteralFalse:
                        case TokenCppKind_LiteralCharacter:
                        case TokenCppKind_LiteralCharacterWide:
                        case TokenCppKind_LiteralCharacterUTF8:
                        case TokenCppKind_LiteralCharacterUTF16:
                        case TokenCppKind_LiteralCharacterUTF32:
                        {
                            color = DATA_COLOR;
                        } break;
                    }
                } break;
            }
            
            paint_text_color(app, text_layout_id, Ii64_size(token->pos, token->size), color);            
            
            if(token->kind == TokenBaseKind_Comment)
            {
                String_Const_u8 comment = push_token_lexeme(app, scratch, buffer, token);
                for(i64 index = token->pos; comment.size > 0; comment = string_skip(comment, 1), index++)
                {
                    for(i64 pair_index = 0; pair_index < ArrayCount(pairs); pair_index++)
                    {
                        Comment_Highlight_Pair* pair = pairs + pair_index;
                        u64 needle_size = pair->needle.size;
                        if(needle_size == 0) continue;
                        
                        String_Const_u8 prefix = string_prefix(comment, needle_size);
                        if(string_match(prefix, pair->needle))
                        {
                            Range_i64 range = Ii64_size(index, needle_size);
                            paint_text_color(app, text_layout_id, range, pair->color);
                            comment = string_skip(comment, needle_size-1);
                            index += needle_size-1;
                            break;                            
                        }
                    }
                }
            }
            
            if(!token_it_inc_all(&iterator))
                break;
        }
        
        //draw_cpp_token_colors(app, text_layout_id, &token_array);
        
        //draw_comment_highlights(app, buffer, text_layout_id, &token_array, pairs, ArrayCount(pairs));        
    }
    else
    {        
        paint_text_color_fcolor(app, text_layout_id, visible_range, fcolor_id(defcolor_text_default));
    }
}

function void
draw_cursor(Application_Links* app, View_ID view_id, Text_Layout_ID text_layout_id, f32 roundness)
{
    i64 cursor_pos = view_get_cursor_pos(app, view_id);
    draw_character_block(app, text_layout_id, cursor_pos, roundness, fcolor_id(defcolor_cursor));
    paint_text_color_pos(app, text_layout_id, cursor_pos, fcolor_id(defcolor_at_cursor));
}

function void
render_buffer(Application_Links *app, View_ID view_id, Face_ID face_id,
              Buffer_ID buffer, Text_Layout_ID text_layout_id,
              Rect_f32 rect){
    ProfileScope(app, "render buffer");
    
    //View_ID active_view = get_active_view(app, Access_Always);
    //b32 is_active_view = (active_view == view_id);
    Rect_f32 prev_clip = draw_set_clip(app, rect);        
    
    draw_cpp_tokens(app, buffer, text_layout_id);
    
    i64 cursor_pos = view_correct_cursor(app, view_id);
    view_correct_mark(app, view_id);
    
    // NOTE(allen): Scope highlight        
    u32 scope_color = SCOPE_HIGHLIGHT_COLOR;
    draw_scope_highlight(app, buffer, text_layout_id, cursor_pos, &scope_color, 1);    
    
    // NOTE(allen): Error highlight    
    String_Const_u8 name = string_u8_litexpr("*compilation*");
    Buffer_ID compilation_buffer = get_buffer_by_name(app, name, Access_Always);    
    draw_jump_highlights(app, buffer, text_layout_id, compilation_buffer,
                         make_color(ERROR_HIGHLIGHT_COLOR));    
    
    // NOTE(allen): Color parens        
    u32 paren_color = GREENISH_COLOR;
    draw_paren_highlight(app, buffer, text_layout_id, cursor_pos, &paren_color, 1);    
    
    u32 line_color = LINE_COLOR;
    if(global_mark)
        line_color = LINE_MARK_COLOR;
    
    // NOTE(allen): Line highlight    
    i64 line_number = get_line_number_from_pos(app, buffer, cursor_pos);
    draw_line_highlight(app, text_layout_id, line_number,
                        line_color);
    
    // NOTE(allen): Cursor shape
    Face_Metrics metrics = get_face_metrics(app, face_id);
    f32 cursor_roundness = (metrics.normal_advance*0.5f)*0.9f;
    //f32 mark_thickness = 2.f;
    
    if(global_macro_recording)
    {
        Rect_f32 rect = view_get_buffer_region(app, view_id);
        
        //TODO(JJ): There can be some better logic for the recording/not recording aspect
        draw_string(app, face_id, string_u8_litexpr("Recording"), V2f32(rect.x1-80.0f, rect.y0+5.0f), REDISH_COLOR);
    }
    
    draw_mark_range(app, view_id, text_layout_id, cursor_roundness);
    
    draw_cursor(app, view_id, text_layout_id, cursor_roundness);    
    
    // NOTE(allen): put the actual text on the actual screen
    draw_text_layout_default(app, text_layout_id);        
    
    draw_set_clip(app, prev_clip);
}

function void
render_caller(Application_Links *app, Frame_Info frame_info, View_ID view_id)
{
    ProfileScope(app, "default render caller");
    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);
    
    Rect_f32 region = draw_background_and_margin(app, view_id, is_active_view);
    Rect_f32 prev_clip = draw_set_clip(app, region);
    
    Buffer_ID buffer = view_get_buffer(app, view_id, Access_Always);
    Face_ID face_id = get_face_id(app, buffer);
    Face_Metrics face_metrics = get_face_metrics(app, face_id);
    f32 line_height = face_metrics.line_height;    
    
    String_Const_u8 name = string_u8_litexpr("*compilation*");
    Buffer_ID compilation_buffer = get_buffer_by_name(app, name, Access_Always);    
    
    if(buffer != compilation_buffer)
    {
        Rect_f32_Pair pair = layout_file_bar_on_top(region, line_height);
        draw_file_bar(app, view_id, buffer, face_id, pair.min);
        region = pair.max;    
    }
    
    Buffer_Scroll scroll = view_get_buffer_scroll(app, view_id);    
    Buffer_Point_Delta_Result delta = delta_apply(app, view_id,
                                                  frame_info.animation_dt, scroll);
    
    if (!block_match_struct(&scroll.position, &delta.point)){
        block_copy_struct(&scroll.position, &delta.point);
        view_set_buffer_scroll(app, view_id, scroll, SetBufferScroll_NoCursorChange);
    }
    if (delta.still_animating){
        animate_in_n_milliseconds(app, 0);
    }
    
    // NOTE(allen): query bars
    {
        Query_Bar *space[32];
        Query_Bar_Ptr_Array query_bars = {};
        query_bars.ptrs = space;
        if (get_active_query_bars(app, view_id, ArrayCount(space), &query_bars)){
            for (i32 i = 0; i < query_bars.count; i += 1){
                Rect_f32_Pair pair = layout_query_bar_on_top(region, line_height, 1);
                draw_query_bar(app, query_bars.ptrs[i], face_id, pair.min);
                region = pair.max;
            }
        }
    }
    
    // NOTE(allen): begin buffer render
    Buffer_Point buffer_point = scroll.position;
    Text_Layout_ID text_layout_id = text_layout_create(app, buffer, region, buffer_point);
    
    // NOTE(allen): draw the buffer
    render_buffer(app, view_id, face_id, buffer, text_layout_id, region);
    
    text_layout_free(app, text_layout_id);
    draw_set_clip(app, prev_clip);
}

BUFFER_HOOK_SIG(begin_buffer)
{    
    Scratch_Block scratch(app);
    
    Language_Type language = {};    
    String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer_id);
    if(file_name.size > 0)
    {
        String_Const_u8 ext = string_file_extension(file_name);
        if(string_match(ext, string_u8_litexpr("cpp")) ||
           string_match(ext, string_u8_litexpr("h")) ||
           string_match(ext, string_u8_litexpr("c")) ||
           string_match(ext, string_u8_litexpr("hpp")) ||
           string_match(ext, string_u8_litexpr("cc")) ||
           string_match(ext, string_u8_litexpr("shader")) ||
           string_match(ext, string_u8_litexpr("glsl")) ||
           string_match(ext, string_u8_litexpr("hlsl")))
        {
            language = LANGUAGE_TYPE_CPP;
        }        
    }
    
    Command_Map_ID map_id = (language == LANGUAGE_TYPE_UNKNOWN) ? mapid_file : mapid_code;
    Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
    Command_Map_ID* map_id_ptr = scope_attachment(app, scope, buffer_map_id, Command_Map_ID);
    *map_id_ptr = map_id;    
    
    Line_Ending_Kind setting = guess_line_ending_kind_from_buffer(app, buffer_id);
    Line_Ending_Kind *eol_setting = scope_attachment(app, scope, buffer_eol_setting, Line_Ending_Kind);
    *eol_setting = setting;
    
    b32 wrap_lines = false;
    b32 use_lexer = (language != LANGUAGE_TYPE_UNKNOWN);
    
    if(use_lexer)
    {
        Async_Task *lex_task_ptr = scope_attachment(app, scope, buffer_lex_task, Async_Task);
        *lex_task_ptr = async_task_no_dep(&global_async_system, do_full_lex_async, make_data_struct(&buffer_id));
    }
    
    b32 *wrap_lines_ptr = scope_attachment(app, scope, buffer_wrap_lines, b32);
    *wrap_lines_ptr = wrap_lines;
    
    buffer_set_layout(app, buffer_id, layout_generic);
    
    return 0;
}

CUSTOM_COMMAND_SIG(vsplit_view)
{
    View_ID view = get_active_view(app, Access_Always);
    View_ID new_view = open_view(app, view, ViewSplit_Right);
    view_settings(app, new_view);    
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
    view_set_buffer(app, new_view, buffer, 0);
}

CUSTOM_COMMAND_SIG(startup)
{
    User_Input input = get_current_input(app);
    if(match_core_code(&input, CoreCode_Startup))
    {        
        Thread_Context* Context = get_thread_context(app);
        heap_init(&global_heap, Context->allocator);
        
        Buffer_Identifier right_buffer = buffer_identifier(string_u8_litexpr("*scratch*"));
        Buffer_ID right_buffer_id = buffer_identifier_to_id(app, right_buffer);
        
        View_ID left_view = get_active_view(app, Access_Always);
        view_settings(app, left_view);        
        vsplit_view(app);
        
        View_ID right_view = get_active_view(app, Access_Always);
        view_set_buffer(app, right_view, right_buffer_id, 0);
        
        view_set_active(app, left_view);
        
        load_project(app);
    }
}

CUSTOM_COMMAND_SIG(try_exit)
{
    User_Input input = get_current_input(app);
    if(match_core_code(&input, CoreCode_TryExit))
    {
        b32 exit = true;
        b32 has_unsaved_changes = false;
        
        for(Buffer_ID buffer = get_buffer_next(app, 0, Access_Always); buffer != 0; buffer = get_buffer_next(app, buffer, Access_Always))
        {
            Dirty_State dirty = buffer_get_dirty_state(app, buffer);
            if(HasFlag(dirty, DirtyState_UnsavedChanges))
            {
                has_unsaved_changes = true; 
                break;
            }
        }
        
        if(has_unsaved_changes)
        {                        
            Scratch_Block scratch(app);
            Lister_Choice_List list = {};
            lister_choice(scratch, &list, "No (N)", "", KeyCode_N, SureToKill_No);
            lister_choice(scratch, &list, "Yes (Y)", "", KeyCode_Y, SureToKill_Yes);
            lister_choice(scratch, &list, "Save all and close (S)", "", KeyCode_S, SureToKill_Save);
            
            Lister_Choice* Choice = get_choice_from_user(app, "There are one or more buffers with unsaved changes, close anyway?", list);
            
            if(Choice != NULL)
            {
                switch(Choice->user_data)
                {
                    case SureToKill_No:
                    {
                        exit = false;
                    } break;
                    
                    case SureToKill_Save:
                    {
                        save_all_dirty_buffers(app);            
                    } break;
                }
            }
        }
        
        if(exit)        
        {
            leave_current_input_unhandled(app);
            exit_4coder(app);
        }
    }
}

global b32 global_build_panel_is_on;
CUSTOM_COMMAND_SIG(close_panel_or_build_panel)
{
    if(global_build_panel_is_on)
    {
        close_build_panel(app);
        global_build_panel_is_on = false;
    }
    else    
        close_panel(app);    
}

CUSTOM_COMMAND_SIG(build)
{
    build_in_build_panel(app);
    global_build_panel_is_on = true;
}

CUSTOM_COMMAND_SIG(jj_query_replace)
{
    if(global_mark)
    {
        query_replace_selection(app);
        global_mark = false;
    }    
    else
    {
        query_replace(app);
    }
}

function void
tick(Application_Links *app, Frame_Info frame_info)
{
    Scratch_Block scratch(app);    
    for (Buffer_Modified_Node *node = global_buffer_modified_set.first; node != 0; node = node->next)
    {
        Temp_Memory_Block temp(scratch);
        Buffer_ID buffer_id = node->buffer;
        
        Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
        
        String_Const_u8 contents = push_whole_buffer(app, scratch, buffer_id);
        Token_Array *tokens_ptr = scope_attachment(app, scope, attachment_tokens, Token_Array);
        if (tokens_ptr == 0)
            continue;        
        if (tokens_ptr->count == 0)
            continue;
        
        Token_Array tokens = *tokens_ptr;
        
        Arena arena = make_arena_system(KB(16));
        Code_Index_File *index = push_array_zero(&arena, Code_Index_File, 1);
        index->buffer = buffer_id;
        
        Generic_Parse_State state = {};
        generic_parse_init(app, &arena, contents, &tokens, &state);
        // TODO(allen): Actually determine this in a fair way.
        // Maybe switch to an enum.
        state.do_cpp_parse = true;
        generic_parse_full_input_breaks(index, &state, max_i32);
        
        code_index_lock();
        code_index_set_file(buffer_id, arena, index);
        
        for(i32 note_index = 0; note_index < index->note_array.count; note_index++)
        {
            Code_Index_Note* note = index->note_array.ptrs[note_index];
            switch(note->note_kind)
            {
                case CodeIndexNote_Type:
                case CodeIndexNote_Function:
                case CodeIndexNote_Macro:
                {                                           
                    add_color_lexer_entry(note->text, note->note_kind);                                       
                } break;                
            }
        }
        
        code_index_unlock();
        buffer_clear_layout_cache(app, buffer_id);
    }
    
    buffer_modified_set_clear();
}

void setup_color_scheme(Application_Links* app)
{
    if(global_theme_arena.base_allocator == 0)    
        global_theme_arena = make_arena_system();        
    Arena* arena = &global_theme_arena;
    
#define COLOR(color) make_colors(arena, color)
    
    default_color_table = make_color_table(app, arena);    
    default_color_table.arrays[0] = COLOR(BLACK_COLOR);
    default_color_table.arrays[defcolor_bar] = COLOR(BAR_COLOR);
    default_color_table.arrays[defcolor_back] = COLOR(BLACK_COLOR);
    
    default_color_table.arrays[defcolor_line_numbers_text] = COLOR(FOREGROUND_COLOR);
    
    default_color_table.arrays[defcolor_list_item] = COLOR(LIST_COLOR);
    default_color_table.arrays[defcolor_list_item_hover] = COLOR(HIGHLIGHT_COLOR);
    default_color_table.arrays[defcolor_list_item_active] = COLOR(HIGHLIGHT_COLOR);
    default_color_table.arrays[defcolor_text_default] = COLOR(FOREGROUND_COLOR);
    default_color_table.arrays[defcolor_keyword] = COLOR(ORANGISH_COLOR);
    default_color_table.arrays[defcolor_str_constant] = COLOR(DATA_COLOR);
    default_color_table.arrays[defcolor_char_constant] = COLOR(DATA_COLOR);
    default_color_table.arrays[defcolor_int_constant] = COLOR(DATA_COLOR);
    default_color_table.arrays[defcolor_float_constant] = COLOR(DATA_COLOR);
    default_color_table.arrays[defcolor_bool_constant] = COLOR(DATA_COLOR);
    default_color_table.arrays[defcolor_comment] = COLOR(COMMENT_COLOR);
    default_color_table.arrays[defcolor_mark] = COLOR(BLACK_COLOR);
    default_color_table.arrays[defcolor_cursor] = COLOR(WHITE_COLOR);
    default_color_table.arrays[defcolor_at_cursor] = COLOR(BLACK_COLOR);
    default_color_table.arrays[defcolor_include] = COLOR(FOREGROUND_COLOR);
    default_color_table.arrays[defcolor_preproc] = COLOR(FOREGROUND_COLOR);    
    default_color_table.arrays[defcolor_base] = COLOR(FOREGROUND_COLOR);
    default_color_table.arrays[defcolor_pop1] = COLOR(FOREGROUND_COLOR);
    default_color_table.arrays[defcolor_pop2] = COLOR(REDISH_COLOR);
    default_color_table.arrays[defcolor_highlight] = COLOR(SEARCH_HIGHLIGHT_COLOR);
    default_color_table.arrays[defcolor_at_highlight] = COLOR(FOREGROUND_COLOR);
    
#undef COLOR
    active_color_table = default_color_table;
}

void setup_hooks(Application_Links* app)
{
    set_custom_hook(app, HookID_BufferViewerUpdate, default_view_adjust);    
    set_custom_hook(app, HookID_ViewEventHandler, default_view_input_handler);
    set_custom_hook(app, HookID_Tick, tick);
    set_custom_hook(app, HookID_RenderCaller, render_caller);
    set_custom_hook(app, HookID_DeltaRule, fixed_time_cubic_delta); set_custom_hook_memory_size(app, HookID_DeltaRule, delta_ctx_size(fixed_time_cubic_delta_memory_size));    
    set_custom_hook(app, HookID_BufferNameResolver, default_buffer_name_resolution);    
    set_custom_hook(app, HookID_BeginBuffer, begin_buffer);
    set_custom_hook(app, HookID_EndBuffer, end_buffer_close_jump_list);
    set_custom_hook(app, HookID_NewFile, default_new_file);
    set_custom_hook(app, HookID_SaveFile, default_file_save);
    set_custom_hook(app, HookID_BufferEditRange, default_buffer_edit_range);
    set_custom_hook(app, HookID_BufferRegion, default_buffer_region);    
    set_custom_hook(app, HookID_Layout, layout_unwrapped);
}
void setup_mapping(Mapping* mapping, i64 global_id, i64 file_id, i64 code_id)
{
    MappingScope();
    SelectMapping(mapping);
    
    SelectMap(global_id);
    BindCore(startup, CoreCode_Startup);
    BindCore(try_exit, CoreCode_TryExit);    
    BindMouseWheel(mouse_wheel_scroll);
    BindMouseWheel(mouse_wheel_change_face_size, KeyCode_Control);
    Bind(change_active_panel, KeyCode_P, KeyCode_Alt);
    Bind(interactive_open_or_new, KeyCode_F, KeyCode_Alt);    
    Bind(interactive_switch_buffer, KeyCode_O, KeyCode_Alt);    
    Bind(kill_buffer, KeyCode_K, KeyCode_Alt);
    Bind(goto_next_jump, KeyCode_N, KeyCode_Alt);
    Bind(goto_prev_jump, KeyCode_N, KeyCode_Alt, KeyCode_Shift);
    Bind(save_all_dirty_buffers, KeyCode_S, KeyCode_Alt, KeyCode_Shift);
    Bind(build, KeyCode_B, KeyCode_Alt);    
    Bind(command_lister, KeyCode_X, KeyCode_Alt);
    Bind(exit_4coder, KeyCode_F4, KeyCode_Alt);
    Bind(close_panel_or_build_panel, KeyCode_C, KeyCode_Alt);  
    Bind(search, KeyCode_M, KeyCode_Control);
    Bind(reverse_search, KeyCode_N, KeyCode_Control);
    Bind(toggle_macro, KeyCode_0, KeyCode_Control);
    Bind(replay_macro, KeyCode_0, KeyCode_Alt);
    
    SelectMap(file_id);
    ParentMap(global_id);            
    BindTextInput(write_text_input);    
    BindMouse(click_set_cursor_and_mark, MouseCode_Left);
    BindMouseRelease(click_set_cursor, MouseCode_Left);
    BindCore(click_set_cursor_and_mark, CoreCode_ClickActivateView);
    BindMouseMove(click_set_cursor_if_lbutton);    
    Bind(delete_char, KeyCode_D, KeyCode_Control);
    Bind(backspace_char, KeyCode_S, KeyCode_Control);    
    Bind(backspace_char, KeyCode_Backspace);    
    Bind(move_up_to_blank_line_end, KeyCode_I, KeyCode_Control);
    Bind(move_down_to_blank_line_end, KeyCode_K, KeyCode_Control);
    Bind(move_left_alpha_numeric_boundary, KeyCode_J, KeyCode_Control);
    Bind(move_right_alpha_numeric_boundary, KeyCode_L, KeyCode_Control);
    Bind(jj_set_mark, KeyCode_Space, KeyCode_Control);
    Bind(jj_copy, KeyCode_C, KeyCode_Control);
    Bind(jj_cut, KeyCode_W, KeyCode_Control);
    Bind(paste_and_indent, KeyCode_F, KeyCode_Control);   
    Bind(page_up, KeyCode_Y, KeyCode_Control);
    Bind(page_down, KeyCode_H, KeyCode_Control);
    Bind(goto_beginning_of_file, KeyCode_Y, KeyCode_Control, KeyCode_Alt);
    Bind(goto_end_of_file, KeyCode_H, KeyCode_Control, KeyCode_Alt);
    Bind(seek_beginning_of_line, KeyCode_A, KeyCode_Control);
    Bind(seek_end_of_line, KeyCode_Z, KeyCode_Control);
    Bind(auto_indent_range, KeyCode_Q, KeyCode_Control);
    Bind(undo, KeyCode_U, KeyCode_Control);
    Bind(redo, KeyCode_O, KeyCode_Control);    
    Bind(jj_query_replace, KeyCode_R, KeyCode_Control);
    Bind(jj_kill_to_end_of_line, KeyCode_X, KeyCode_Control);
    Bind(swap_panels, KeyCode_P, KeyCode_Control);
    Bind(save, KeyCode_S, KeyCode_Alt);    
    Bind(vsplit_view, KeyCode_1, KeyCode_Alt);
    Bind(list_all_functions_current_buffer_lister, KeyCode_L, KeyCode_Alt);
    Bind(list_all_functions_all_buffers_lister, KeyCode_L, KeyCode_Alt, KeyCode_Shift);
    Bind(move_up, KeyCode_I, KeyCode_Control, KeyCode_Shift);
    Bind(move_down, KeyCode_K, KeyCode_Control, KeyCode_Shift);
    Bind(move_left, KeyCode_J, KeyCode_Control, KeyCode_Shift);
    Bind(move_right, KeyCode_L, KeyCode_Control, KeyCode_Shift);  
    Bind(move_line_up, KeyCode_I, KeyCode_Control, KeyCode_Alt);
    Bind(move_line_down, KeyCode_K, KeyCode_Control, KeyCode_Alt);             
    Bind(auto_indent_whole_file, KeyCode_Q, KeyCode_Control, KeyCode_Alt);            
    Bind(list_all_locations_lister_substring, KeyCode_F, KeyCode_Control, KeyCode_Shift);
    Bind(list_all_locations_lister, KeyCode_F, KeyCode_Control, KeyCode_Alt);   
    
    SelectMap(code_id);
    ParentMap(file_id);
    BindTextInput(write_text_and_auto_indent);            
    Bind(word_complete, KeyCode_Tab);    
    Bind(word_complete_drop_down, KeyCode_Tab, KeyCode_Control);
    Bind(list_all_locations_of_type_definition, KeyCode_T, KeyCode_Control);
    Bind(list_all_locations_of_type_definition_of_identifier, KeyCode_T, KeyCode_Control, KeyCode_Shift);     
    //setup_default_mapping(mapping, global_id, file_id, code_id);
}

void custom_layer_init(Application_Links *app)
{
    Thread_Context *tctx = get_thread_context(app);
    
    global_lexer_hash_table = init_lexer_hash_table();
    
    // NOTE(allen): setup for default framework
    async_task_handler_init(app, &global_async_system);
    code_index_init();
    buffer_modified_set_init();
    Profile_Global_List *list = get_core_profile_list(app);
    ProfileThreadName(tctx, list, string_u8_litexpr("main"));
    initialize_managed_id_metadata(app);    
    mapping_init(tctx, &framework_mapping);
    
#if 0 
    set_default_color_scheme(app);    
#else
    setup_color_scheme(app);
#endif
    setup_hooks(app);
    setup_mapping(&framework_mapping, mapid_global, mapid_file, mapid_code);    
}