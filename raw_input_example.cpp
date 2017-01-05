


// this is some example code for how to handle raw input
// to get hold of left / right versions of shift contorl alt and enter keys. It also translates to unicode so you don't have to jump through
// the hoops of also listening to wm_unicode and trying to get that to call you correctly


// note also: that we in this case keps track of keystates in application.
// that means that we need to listen to keypresses while where not on top.

// note: this is copied out from a larger program, this is not a working example
// just the parts which matters to understand how to use raw input
// DO NOT USE IN PRODUCTION without modifiying it to actually work like you like it to
// this is be crawling with bugs, we don't handle everythign. 
// Keypress on arrow and release on numpad arrow when num_lock is down will cancel out for example.
// this should be handled like the other left/right things. But you probably get the idea.

// Feel free to use it as you please, but keep in mind the previous paragraph.



LRESULT CALLBACK win32MainWindowCallback(HWND window,
										UINT message,
										WPARAM wParam,
										LPARAM lParam)
{
	LRESULT result = 0;
	switch (message)
	{	
		case WM_INPUT:
		{
			//this documentation is retarded, msdn implies that the size can be any size whatsoever, 
			//however it will be *one* of the *possible* unions, so unless we like to dynamically allocate to save like two bytes.
			//we can just point to our own rawinput structure. Come on MSDN. I dislike allocating shit that I don't have to.

			RAWINPUT raw;
			UINT dw_size = sizeof(RAWINPUT);
			int ret = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &dw_size,sizeof(RAWINPUTHEADER));
			assert(ret > 0);
			RAWKEYBOARD kb = raw.data.keyboard;
			static unsigned char keyState[256] = {};
			
			{	// update our keystate
				// if you don't care about getting the unicode translated thing later you might choose to store it differnetly.
				bool e0 = kb.Flags & RI_KEY_E0;
				bool e1 = kb.Flags & RI_KEY_E1;

				// these are unasigned but not reserved as of now.
				// this is bad but, you know, we'll fix it if it ever breaks.
				#define VK_LRETURN         0x9E
				#define VK_RRETURN         0x9F

				#define UPDATE_KEY_STATE(key) do{keyState[key] = (kb.Flags & 1) ? 0 : 0xff;}while(0)
				// note: we set all bits in the byte if the key is down. 
				// This is becasue windows expects it to be in the high_order_bit (when using it for converting to unicode for example)
				// and I like it to be in the low_order_bit,  
				if (kb.VKey == VK_CONTROL)
				{
					if (e0)	UPDATE_KEY_STATE(VK_RCONTROL);
					else	UPDATE_KEY_STATE(VK_LCONTROL);
					keyState[VK_CONTROL] = keyState[VK_RCONTROL] | keyState[VK_LCONTROL];
				}
				else if (kb.VKey == VK_SHIFT)
				{
					// because why should any api be consistant lol
					// (because we get different scancodes for l/r-shift but not for l/r ctrl etc... but still)
					UPDATE_KEY_STATE(MapVirtualKey(kb.MakeCode, MAPVK_VSC_TO_VK_EX));
					keyState[VK_SHIFT] = keyState[VK_LSHIFT] | keyState[VK_RSHIFT];
				}
				else if (kb.VKey == VK_MENU)
				{
					if (e0)	UPDATE_KEY_STATE(VK_LMENU);
					else	UPDATE_KEY_STATE(VK_RMENU);
					keyState[VK_CONTROL] = keyState[VK_RMENU] | keyState[VK_LMENU];
				}
				else if (kb.VKey == VK_RETURN)
				{
					if (e0) UPDATE_KEY_STATE(VK_RRETURN);
					else	UPDATE_KEY_STATE(VK_LRETURN);
					keyState[VK_RETURN] = keyState[VK_RRETURN] | keyState[VK_LRETURN];
				}
				else
				{
					UPDATE_KEY_STATE(kb.VKey);
				}
				#undef UPDATE_KEY_STATE
			}

			bool window_is_ontop = !wParam;
			if (raw.header.dwType == RIM_TYPEKEYBOARD && window_is_ontop)
			{
				if(!(kb.Flags & 0x1)) //down
				{
					char utf8_buffer[32];
					char *buff=0;
					int utf8_len = 0;
					
					// get unicode.
					wchar_t utf16_buffer[16];
					int utf16_len = ToUnicode(kb.VKey, kb.MakeCode, keyState, utf16_buffer, ARRAY_LENGTH(utf16_buffer), 0);
					
					// get some values
					bool shift_left    = keyState[VK_LSHIFT];
					bool shift_right   = keyState[VK_RSHIFT];
					bool control_right = keyState[VK_RCONTROL];
					bool control_left  = keyState[VK_LCONTROL];
					bool alt_right     = keyState[VK_RMENU];
					bool alt_left      = keyState[VK_LMENU];

					result = DefWindowProc(window, message, wParam, lParam);
				}
			}
			break;
		}
		case WM_DESTROY: 
		{//kill application
			running = false;
			break;
		}
		default:
		{
			result = DefWindowProc(window,message,wParam,lParam);
		} break;
	}
	return result;	
}



int CALLBACK
WinMain(HINSTANCE instance,
		HINSTANCE prevInstance,
		LPSTR args,
		int showCode)
{

	//create the window

	WNDCLASS windowClass = {};
	windowClass.hIcon = (HICON)iconSmall;
	windowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
	windowClass.lpfnWndProc = win32MainWindowCallback;
	windowClass.hInstance = instance;
	windowClass.lpszClassName = "TestWindowClass";
	
	// register window
	RegisterClass(&windowClass);
	HWND window = CreateWindowEx(
		0,	windowClass.lpszClassName,
		"Test", WS_OVERLAPPEDWINDOW|WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		0, 0, instance, 0);
	
	//register to listen to raw input. also when we're not active!	
	RAWINPUTDEVICE rid = {};
	rid.usUsagePage = 0x01;
	rid.usUsage = 0x06;
	rid.dwFlags = RIDEV_NOLEGACY| RIDEV_INPUTSINK; // input sink is marking us for listeningn in the background. Sneeky   
	rid.hwndTarget = window;

	assert(RegisterRawInputDevices(&rid, 1, sizeof(rid)));
}	