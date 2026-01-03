//
// MIT License
//
// Copyright 2025 KAOSS <thekaossphere@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

// Needed for the callbacks
#include <functional>

// Needed for undo stack
#include <vector>

// Core definitions for the LV2 interface
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

//
// Configuration constants
//

/// URI which identifies the plugin
static const char* LOOPER_URI = "https://github.com/theKAOSSphere/one-button-looper/";
/// The maximum number of dubs that can be recorded
static const size_t NR_OF_DUBS = 128;
/// The maximum number of seconds which can be recorded for all dubs.
/// Note that each dub can have an individual length. If audio starts
/// after the loop start and/or finishes before the end of the loop
/// it will consume less memory.
static const size_t STORAGE_MEMORY_SECONDS = 360;
static const size_t NR_OF_BLEND_SAMPLES = 64;
/// Time threshold for double click detection (in seconds)
static const double DOUBLE_CLICK_TIME = 0.3;
/// Time threshold for hold detection (in seconds)
static const double HOLD_TIME = 1.5;

///
/// Convert an input parameter expressed as db into a linear float value
///
static float dbToFloat(float db)
{
    if (db <= -90.0f)
        return 0.0f;
    return powf(10.0f, db * 0.05f);
}

///
/// Represent the state of the dub
///
typedef enum
{
    // The looper does not contain any recorded loop.
    LOOPER_STATE_EMPTY,
    // The looper has started recording a dub, but audio did not exceed the
    // threshold so far.
    LOOPER_STATE_WAITING_FOR_THRESHOLD,
    // The looper is recording a dub.
    LOOPER_STATE_RECORDING,
    // The looper has recorded a loop but is currently not playing.
    LOOPER_STATE_STOPPED,
    // The looper is still playing all the active dubs.
    LOOPER_STATE_PLAYING,
    // The looper is overdubbing (recording additional layers)
    LOOPER_STATE_OVERDUBBING
} State;

///
/// The indices for the ports we support
///
enum PortIndex
{
    /// Audio input 1
    LOOPER_INPUT1 = 0,
    /// Audio input 2
    LOOPER_INPUT2 = 1,
    /// Audio output 1
    LOOPER_OUTPUT1 = 2,
    /// Audio output 2
    LOOPER_OUTPUT2 = 3,
    /// Threshold parameter
    LOOPER_THRESHOLD = 4,
    /// Main control button (Ditto-style)
    LOOPER_MAIN_BUTTON = 5,
    /// Amount of the dry signal in the output
    LOOPER_DRY_AMOUNT = 6,
    /// Select if dub continues across loop boundaries
    LOOPER_CONTINUOUS_DUB = 7,
    /// Output port for MODGUI state monitoring (0-5)
    LOOPER_STATE_OUTPUT = 8,    
    /// Output port for number of dubs
    LOOPER_DUB_COUNT_OUTPUT = 9
};

///
/// Represent a dub
///
class Dub
{
public:
    /// Where is the dub's audio memory starting in the global audio storage?
    size_t m_storageOffset = 0;
    /// The length of the dub. Each dub can have an individual length, but they
    /// will still stay in sync!
    size_t m_length = 0;
    /// The start index in the loop. This allows to save the memory before there
    /// is actual audio in the loop. A dub only needs the memory between the
    /// first and the last audio saved in the dub.
    size_t m_startIndex = 0;
};

///
/// Enhanced button handler for Ditto-style functionality with tap detection
/// - Tap actions fire on PRESS for instant response
/// - Hold fires after 1.5s and REVERTS the tap action
/// - Double-tap detected by timing between consecutive releases
///
class DittoButton
{
public:
    // Connect the button to an input and set the callback.
    // Callback signature: (Action Type, isPress)
    // 1 = Single Tap (Rec/Play/Dub), 2 = Double Tap (Stop), 3 = Hold (Undo/Clear), 4 = Tap Release
    // isPress = true means this tap might become a hold
    void connect(void* input, std::function<void (int, bool)> callback)
    {
        m_input = static_cast<const float*>(input);
        m_callback = callback;
    }

    /// To be called every run call of the plugin. Will check the state of the
    /// button and call the callback if necessary.
    ///
    /// \param now The current time. Used for checking tap timing.
    void run(double now)
    {
        if (m_input == NULL)
            return;

        bool currentState = (*m_input) > 0.0f ? true : false;
        
        // Handle button press (rising edge)
        if (currentState && !m_lastState)
        {
            m_pressStartTime = now;
            m_isPressed = true;
            m_holdTriggered = false;

            // Fire tap IMMEDIATELY on press for instant response
            // Second param = true means "this is a press, might become hold"
            m_callback(1, true);
        }
        
        // Handle button hold (HIGH state)
        else if (currentState && m_isPressed)
        {
            if (!m_holdTriggered && (now - m_pressStartTime) >= HOLD_TIME)
            {
                m_holdTriggered = true;
                // Fire hold - the handler should UNDO the tap action if needed
                m_callback(3, false);
            }
        }
        
        // Handle button release (falling edge)
        else if (!currentState && m_lastState)
        {
            m_lastState = false;
            
            if (m_holdTriggered)
            {
                // Hold already fired, don't do anything else
                return;
            }

            // Check for double-tap (second release within window)
            if (m_lastReleaseTime > 0 && (now - m_lastReleaseTime) < DOUBLE_CLICK_TIME)
            {
                // Fire double-tap: STOP
                m_callback(2, false);
                m_lastReleaseTime = 0; // Reset to prevent triple-tap issues
            }
            else
            {
                // Single tap confirmed on release
                m_callback(4, false); // TAP RELEASE: for delayed actions
                m_lastReleaseTime = now;
            }
        }
    }

    /// The callback
    /// \param action The action type
    /// \param isPress True if this is a press that might become a hold
    std::function<void (int, bool)> m_callback;
    
private:
    /// The input it is connected to.
    const float* m_input = NULL;
    /// The last state, used for detecting changes.
    bool m_lastState = false;
    /// Current press state
    bool m_isPressed = false;
    /// When the current press started
    double m_pressStartTime = 0;
    /// When the last release happened
    double m_lastReleaseTime = 0;
    /// Check if the button is currently being held
    bool m_holdTriggered = false;
};

///
/// The looper class
///
class Looper
{
public:
    /// Constructor
    /// \param sampleRate The sample rate is used for calculation of the storage needed
    ///                   and also the current time.
    Looper(double sampleRate)
        : m_sampleRate(sampleRate)
    {
        // Allocate the needed memory
        m_storageSize = sampleRate * STORAGE_MEMORY_SECONDS * 2;
        m_storage1 = new float[m_storageSize];
        m_storage2 = new float[m_storageSize];
    }

    // Destructor
    ~Looper()
    {
        delete[] m_storage1;
        delete[] m_storage2;
    }

    /// Called by the host for each port to connect it to the looper.
    /// \param port The index of the port to be connected.
    /// \param data A pointer to the data where the parameter will be written to.
    void connectPort(PortIndex port, void* data)
    {
        // Install the trivial ports
        switch (port)
        {
            case LOOPER_INPUT1: m_input1 = (const float*)data; return;
            case LOOPER_INPUT2: m_input2 = (const float*)data; return;
            case LOOPER_OUTPUT1: m_output1 = (float*)data; return;
            case LOOPER_OUTPUT2: m_output2 = (float*)data; return;
            case LOOPER_THRESHOLD: m_thresholdParameter = (const float*)data; return;
            case LOOPER_DRY_AMOUNT: m_dryAmountParameter = (const float*)data; return;
            case LOOPER_CONTINUOUS_DUB: m_continuousDubParameter = (const float*)data; return;
            case LOOPER_STATE_OUTPUT: m_stateOutput = (float*)data; return;
            case LOOPER_DUB_COUNT_OUTPUT: m_dubCountOutput = (float*)data; return;
            default: break;
        }

        // Install the main button with Ditto-style behavior
        if (port == LOOPER_MAIN_BUTTON)
        {
            m_mainButton.connect(data, [this](int action, bool isPress)
            {
                handleDittoButton(action, isPress);
            });
        }
    }

    /// Run the looper. Called for a bunch of samples at a time. Parameters will not change within this
    /// call!
    /// \param The number of samples to be read from the input and written to the output.
    void run(uint32_t nrOfSamples)
    {
        updateParameters();

         // Update monitored outputs for MODGUI
        if (m_stateOutput) *m_stateOutput = static_cast<float>(m_state);
        if (m_dubCountOutput) *m_dubCountOutput = static_cast<float>(m_nrOfDubs);

        m_now += double(nrOfSamples) / m_sampleRate;

        // EMPTY or STOPPED states: just pass through dry signal
        if (m_state == LOOPER_STATE_EMPTY || m_state == LOOPER_STATE_STOPPED)
        {
            for (uint32_t s = 0; s < nrOfSamples; ++s)
            {
                m_output1[s] = m_dryAmount * m_input1[s];
                m_output2[s] = m_dryAmount * m_input2[s];
            }
            return;
        }

        for (uint32_t s = 0; s < nrOfSamples; ++s)
        {
            // Use the live input
            float in1 = m_input1[s];
            float in2 = m_input2[s];

            // Check if we reached the threshold to start recording.
            if (m_state == LOOPER_STATE_WAITING_FOR_THRESHOLD && (fabs(in1) >= m_threshold || fabs(in2) >= m_threshold))
            {
                Dub& dub = m_dubs[m_nrOfDubs];
                dub.m_startIndex = m_currentLoopIndex;
                m_state = LOOPER_STATE_RECORDING;
            }

            // If we are recording or overdubbing, do the record.
            if (m_state == LOOPER_STATE_RECORDING || m_state == LOOPER_STATE_OVERDUBBING)
            {
                // CRITICAL FIX: Bounds check BEFORE writing
                if (m_nrOfUsedSamples < m_storageSize)
                {
                    if (m_state == LOOPER_STATE_OVERDUBBING)
                    {
                        // FIXED: Use '=' not '+=' to avoid ghost audio from undone takes
                        m_storage1[m_nrOfUsedSamples] = in1;
                        m_storage2[m_nrOfUsedSamples] = in2;
                    }
                    else
                    {
                        // For initial recording, replace audio
                        m_storage1[m_nrOfUsedSamples] = in1;
                        m_storage2[m_nrOfUsedSamples] = in2;
                    }
                    m_nrOfUsedSamples++;
                    Dub& dub = m_dubs[m_nrOfDubs];
                    dub.m_length++;
                }
                else
                {
                    // Storage full, finish recording/overdubbing
                    if (m_state == LOOPER_STATE_RECORDING)
                        finishRecording();
                    else if (m_state == LOOPER_STATE_OVERDUBBING)
                        finishOverdubbing();
                }
            }

            // Playback all active dubs.
            float out1 = m_dryAmount * in1;
            float out2 = m_dryAmount * in2;
            for (size_t t = 0; t < m_nrOfDubs; t++)
            {
                Dub& dub = m_dubs[t];
                if (m_currentLoopIndex < dub.m_startIndex)
                    continue;
                if (m_currentLoopIndex >= dub.m_startIndex + dub.m_length)
                    continue;
                size_t index = dub.m_storageOffset + (m_currentLoopIndex - dub.m_startIndex);
                out1 += m_storage1[index];
                out2 += m_storage2[index];
            }

            // Store accumulated output.
            m_output1[s] = out1;
            m_output2[s] = out2;

            if (m_nrOfDubs > 0)
                // Only once we are actually playing anything the loop length is known.
                m_currentLoopIndex++;

            // LOOP END CHECK
            // Only check loop end if we actually have a defined loop length (m_loopLength > 0)
            // During first recording, m_loopLength is 0 - we don't want to trigger end-of-loop!
            if ((m_loopLength > 0 && m_currentLoopIndex >= m_loopLength) || m_nrOfUsedSamples >= m_storageSize)
            {
                // Reached the end of the loop, either because we exhausted storage
                // or the end of the loop is there.
                m_currentLoopIndex = 0;

                if (m_state == LOOPER_STATE_RECORDING)
                {
                    // Auto-finish the first recording and start playing
                    finishRecording();
                }
                else if (m_state == LOOPER_STATE_OVERDUBBING)
                {
                    // Auto-finish overdubbing and continue playing (or continue if continuous mode)
                    finishOverdubbing();
                    
                    // Check for continuous dub mode
                    if (*m_continuousDubParameter > 0.0f)
                    {
                        // Continue overdubbing automatically across loop boundary
                        startOverdubbing();
                    }
                }
            }
        }
    }

private:
    //
    // Input parameters
    //

    /// Threshold parameter
    const float* m_thresholdParameter = NULL;

    /// Dry amount parameter
    const float* m_dryAmountParameter = NULL;

    /// Continuous dub mode parameter
    const float* m_continuousDubParameter = NULL;
    
    /// Main Ditto-style button
    DittoButton m_mainButton;

    //
    // All audio inputs
    //

    /// Audio input 1
    const float* m_input1 = NULL;
    /// Audio input 2
    const float* m_input2 = NULL;

    //
    // All audio outputs
    //

    /// Audio output 1
    float* m_output1 = NULL;
    /// audio output 2
    float* m_output2 = NULL;

    //
    // Monitored outputs for MODGUI
    //
    /// Output for looper state monitoring
    float* m_stateOutput = NULL;
    /// Output for dub count monitoring
    float* m_dubCountOutput = NULL;

    //
    // Internal state
    //

    /// The stored sample rate
    uint32_t m_sampleRate = 48000;
    /// The current looper state
    State m_state = LOOPER_STATE_EMPTY;
    /// The stored threshold as a linear value
    float m_threshold = 0.0f;
    /// The stored dry amount
    float m_dryAmount = 1.0f;
    /// Where are we with the first (main) loop. The first loop governs all the loops!
    size_t m_currentLoopIndex = 0;
    /// The length of the main loop
    size_t m_loopLength = 0;
    /// Current time, sample accurate used for buttons
    double m_now = 0;
    /// Whether we can undo the last overdub
    bool m_canUndo = false;
    /// Storage offset for undo functionality
    size_t m_undoStorageOffset = 0;
    /// Length for undo functionality  
    size_t m_undoLength = 0;
    /// Whether undo has been toggled (true = currently undone) - commented out, no longer used
    bool m_undoToggled = false;
    // Track state before tap, for hold reversion
    State m_preHoldState = LOOPER_STATE_EMPTY;

    /// Stack for multi-level undo functionality (stores pairs of nrOfDubs and nrOfUsedSamples)
    std::vector<std::pair<size_t, size_t>> m_undoStack;

    //
    // Storage memory for audio
    //

    /// Overall storage size for audio (number of floats per channel)
    size_t m_storageSize = 0;
    /// Number of samples already used
    size_t m_nrOfUsedSamples = 0;
    /// Storage for first channel
    float* m_storage1 = NULL;
    /// Storage for second channel
    float* m_storage2 = NULL;

    //
    // Store information about the dubs
    //

    /// The number of dubs currently active
    size_t m_nrOfDubs = 0;
    /// The number of dubs which we recorded and thus could be redone
    size_t m_maxUsedDubs = 0;
    /// The dubs
    Dub m_dubs[NR_OF_DUBS];

    /// Handle Ditto-style button actions
    /// \param isPress = true means this tap might become a hold (so we might need to undo it)
    /// \param Action 1 = Press, 2 = Double-tap, 3 = Hold, 4 = Release (confirmed tap)
     void handleDittoButton(int action, bool isPress)
    {
        switch (action)
        {
            case 1: // SINGLE TAP - fires on PRESS for instant response
                switch (m_state)
                {
                    case LOOPER_STATE_EMPTY:
                        // No loop - will start recording on release
                        m_preHoldState = LOOPER_STATE_EMPTY;
                        break;
                    case LOOPER_STATE_STOPPED:
                        // Has loop - will resume on release (so hold-to-clear works)
                        m_preHoldState = LOOPER_STATE_STOPPED;
                        break;
                    case LOOPER_STATE_RECORDING:
                    case LOOPER_STATE_WAITING_FOR_THRESHOLD:
                        m_preHoldState = m_state;
                        finishRecording(); 
                        break;
                    case LOOPER_STATE_PLAYING:
                        // Save state in case hold triggers undo
                        m_preHoldState = LOOPER_STATE_PLAYING;
                        startOverdubbing(); 
                        break;
                    case LOOPER_STATE_OVERDUBBING:
                        m_preHoldState = LOOPER_STATE_OVERDUBBING;
                        finishOverdubbing(); 
                        break;
                }
                break;

            case 2: // DOUBLE TAP (Stop)
                m_preHoldState = LOOPER_STATE_EMPTY;
                if (m_state != LOOPER_STATE_EMPTY && m_state != LOOPER_STATE_STOPPED)
                {
                    if (m_state == LOOPER_STATE_RECORDING) finishRecording();
                    else if (m_state == LOOPER_STATE_OVERDUBBING) finishOverdubbing();
                    
                    // Go to STOPPED if we have a loop, EMPTY if not
                    m_state = (m_nrOfDubs > 0) ? LOOPER_STATE_STOPPED : LOOPER_STATE_EMPTY;
                    m_currentLoopIndex = 0;
                }
                break;

            case 3: // HOLD (Undo/Redo or Clear)
                if (m_preHoldState == LOOPER_STATE_EMPTY)
                {
                    // Hold while empty = no-op (already clear)
                }
                else if (m_preHoldState == LOOPER_STATE_STOPPED)
                {
                    // Hold while stopped = CLEAR
                    reset();
                }
                else 
                {
                    // REVERT the tap action that fired on press
                    if (m_preHoldState == LOOPER_STATE_PLAYING && m_state == LOOPER_STATE_OVERDUBBING)
                    {
                        // We started overdubbing on press, cancel it
                        cancelOverdubbing();
                    }
                    else if (m_preHoldState == LOOPER_STATE_OVERDUBBING && m_state == LOOPER_STATE_PLAYING)
                    {
                        // We finished overdubbing on press, undo that finish
                        undoLastOverdub();
                    }
                    
                    // Now do the actual undo/redo toggle
                    if (!m_undoToggled) 
                    {
                        if (!m_undoStack.empty()) 
                        { 
                            undoLastOverdub(); 
                            m_undoToggled = true; 
                        }
                    } 
                    else 
                    {
                        redoLastOverdub(); 
                        m_undoToggled = false;
                    }
                }
                m_preHoldState = LOOPER_STATE_EMPTY; // Reset
                break;

            case 4: // TAP RELEASE - fires on release for delayed actions (EMPTY/STOPPED states)
                if (m_preHoldState == LOOPER_STATE_EMPTY && m_state == LOOPER_STATE_EMPTY)
                {
                    // No loop - start recording
                    startRecording();
                }
                else if (m_preHoldState == LOOPER_STATE_STOPPED && m_state == LOOPER_STATE_STOPPED)
                {
                    // Has loop - resume playback
                    m_state = LOOPER_STATE_PLAYING;
                }
                m_preHoldState = LOOPER_STATE_EMPTY;
                break;
        }
    }

    /// Cancel an in-progress overdub without saving
    void cancelOverdubbing()
    {
        if (m_state != LOOPER_STATE_OVERDUBBING) return;
        
        // Pop the undo stack entry we just pushed
        if (!m_undoStack.empty())
        {
            m_nrOfUsedSamples = m_undoStack.back().second;
            m_undoStack.pop_back();
        }
        
        // Reset the current dub's length
        Dub& dub = m_dubs[m_nrOfDubs];
        dub.m_length = 0;
        
        m_state = LOOPER_STATE_PLAYING;
    }

    /// Reset everything to initial state.
    void reset()
    {
        m_nrOfDubs = 0;
        m_maxUsedDubs = 0;
        m_nrOfUsedSamples = 0;
        m_state = LOOPER_STATE_EMPTY;
        m_currentLoopIndex = 0;
        m_loopLength = 0;
        m_canUndo = false;
        m_undoStorageOffset = 0;
        m_undoLength = 0;
        m_undoToggled = false;
        
        // Clear the undo stack
        m_undoStack.clear();
        
        // FIXED: Removed zeroing of audio buffers to avoid xruns!
        // NOTE: We intentionally do NOT zero the audio buffers here.
        // Zeroing ~276 MB in the real-time thread would cause xruns.
        // The playback logic respects dub boundaries, so old data is never heard.
        // New recordings use '=' not '+=', so they overwrite cleanly.
    }

    /// Start recording a dub if possible (a dub and memory left).
    void startRecording()
    {
        if (m_nrOfDubs >= NR_OF_DUBS)
            // Reached maximum number of dubs, cannot start recording.
            return;
        if (m_nrOfUsedSamples >= m_storageSize)
            // Memory full, cannot start recording.
            return;

        // Prepare the dub.
        Dub& dub = m_dubs[m_nrOfDubs];
        dub.m_storageOffset = m_nrOfUsedSamples;
        dub.m_length = 0;

        // Now start the recording.
        m_state = LOOPER_STATE_WAITING_FOR_THRESHOLD;
    }

    /// Start overdubbing
    void startOverdubbing()
    {
        if (m_nrOfDubs >= NR_OF_DUBS)
            return;
        if (m_nrOfUsedSamples >= m_storageSize)
            return;

        // Push current state to undo stack for multi-level undo
        m_undoStack.push_back({m_nrOfDubs, m_nrOfUsedSamples});

        // Reset undo toggle when beginning a new overdub session
        m_undoToggled = false;

        // Store current state for undo
        m_undoStorageOffset = m_nrOfUsedSamples;
        m_undoLength = 0;
        m_canUndo = false; // Will be set to true when overdub is finished

        // Prepare the new overdub
        Dub& dub = m_dubs[m_nrOfDubs];
        dub.m_storageOffset = m_nrOfUsedSamples;
        dub.m_length = 0;
        dub.m_startIndex = m_currentLoopIndex;

        m_state = LOOPER_STATE_OVERDUBBING;
    }

    /// Finish the recording.
    void finishRecording()
    {
        if (m_state == LOOPER_STATE_WAITING_FOR_THRESHOLD)
        {
            // We did not actually record anything, yet. So nothing to do. Just
            // go back to the previous state.
            if (m_nrOfDubs == 0)
                m_state = LOOPER_STATE_EMPTY;
            else
                m_state = LOOPER_STATE_PLAYING;
            return;
        }

        // We did record something, so make sure we will use it.
        m_state = LOOPER_STATE_PLAYING;
        Dub& dub = m_dubs[m_nrOfDubs];
        if (m_nrOfDubs == 0)
        {
            // This was the first dub which governs the loop length.
            m_loopLength = dub.m_length;
            m_currentLoopIndex = 0;
        }

        // Fixup the start and the end of the loop. We simply fade in and out over
        // NR_OF_BLEND_SAMPLES samples for now.
        size_t length = dub.m_length > NR_OF_BLEND_SAMPLES ? NR_OF_BLEND_SAMPLES : dub.m_length;
        size_t startIndex = dub.m_storageOffset;
        size_t endIndex = dub.m_storageOffset + dub.m_length - 1;
        for (size_t s = 0; s < length; s++)
        {
            float factor = float(s) / length;
            m_storage1[startIndex] *= factor;
            m_storage2[startIndex] *= factor;
            startIndex++;

            // Check to avoid overlapping pointers if dub is extremely short
            if (endIndex > startIndex) {
                m_storage1[endIndex] *= factor;
                m_storage2[endIndex] *= factor;
                endIndex--;
            }
        }

        // Now the dub is officially ready for playing...
        m_nrOfDubs++;

        // Note that when recording a new dub we need to reset max dubs as well, even if
        // once had more dubs: They have been overwritten and cannot be redone!
        m_maxUsedDubs = m_nrOfDubs;
    }

    /// Finish overdubbing
    void finishOverdubbing()
    {
        if (m_state != LOOPER_STATE_OVERDUBBING)
            return;

        m_state = LOOPER_STATE_PLAYING;
        Dub& dub = m_dubs[m_nrOfDubs];

        // Store undo information
        m_undoLength = dub.m_length;
        m_canUndo = true;

        // Apply fade in/out to the overdub
        size_t length = dub.m_length > NR_OF_BLEND_SAMPLES ? NR_OF_BLEND_SAMPLES : dub.m_length;
        size_t startIndex = dub.m_storageOffset;
        size_t endIndex = dub.m_storageOffset + dub.m_length - 1;
        for (size_t s = 0; s < length; s++)
        {
            float factor = float(s) / length;
            m_storage1[startIndex] *= factor;
            m_storage2[startIndex] *= factor;
            startIndex++;
            if (endIndex > startIndex) {
                m_storage1[endIndex] *= factor;
                m_storage2[endIndex] *= factor;
                endIndex--;
            }
        }

        // Activate the overdub
        m_nrOfDubs++;
        m_maxUsedDubs = m_nrOfDubs;
    }

    /// Undo the last overdub
    void undoLastOverdub()
    {
        if (m_undoStack.empty())
            return;

        // Pop the last state from the undo stack
        size_t prevNrOfDubs = m_undoStack.back().first;
        size_t prevNrOfUsedSamples = m_undoStack.back().second;
        m_undoStack.pop_back();

        // Restore the previous state
        m_nrOfDubs = prevNrOfDubs;
        m_nrOfUsedSamples = prevNrOfUsedSamples;

        // If we've undone all overdubs, go back to playing state
        if (m_nrOfDubs > 0)
        {
            m_state = LOOPER_STATE_PLAYING;
        }
        else
        {
            m_state = LOOPER_STATE_EMPTY;
            m_currentLoopIndex = 0;
            m_loopLength = 0;
        }
    }

    /// Redo the last overdub
    void redoLastOverdub()
    {
        if (!m_canUndo || m_nrOfDubs >= m_maxUsedDubs)
            return;

        // Restore the stack
        m_undoStack.push_back({m_nrOfDubs, m_nrOfUsedSamples});
        
        // Reactivate the overdub
        Dub& dub = m_dubs[m_nrOfDubs];
        m_nrOfUsedSamples = dub.m_storageOffset + dub.m_length;
        m_nrOfDubs++;
        m_state = LOOPER_STATE_PLAYING;
    }

    /// Update all the parameters from the inputs.
    void updateParameters()
    {
        m_threshold = dbToFloat(*m_thresholdParameter);
        m_dryAmount = *m_dryAmountParameter;
        m_mainButton.run(m_now);
    }
};

//
// The functions required by the LV2 interface. Simply forward to the Looper class.
//
static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double rate, const char* bundlePath,
    const LV2_Feature* const* features)
{
    return (LV2_Handle)new Looper(rate);
}
static void activate(LV2_Handle instance) {}
static void deactivate(LV2_Handle instance) {}
static void cleanup(LV2_Handle instance) { delete static_cast<Looper*>(instance); }
static const void* extensionData(const char* uri) { return NULL; }
static void connectPort(LV2_Handle instance, uint32_t port, void* data)
{
    Looper* looper = static_cast<Looper*>(instance);
    looper->connectPort(static_cast<PortIndex>(port), data);
}
static void run(LV2_Handle instance, uint32_t nrOfSamples)
{
    Looper* looper = static_cast<Looper*>(instance);
    looper->run(nrOfSamples);
}

///
/// Descriptors for the various functions called by the LV2 host
///
static const LV2_Descriptor descriptor =
{
    /// The URI which identifies the plugin
    LOOPER_URI,
    /// Instantiate the plugin.
    instantiate,
    /// Connect a port, called once for each port.
    connectPort,
    /// Activate the plugin (unused).
    activate,
    /// Process a bunch of samples.
    run,
    /// Deactivate the plugin (unused).
    deactivate,
    /// Cleanup, will destroy the plugin.
    cleanup,
    /// Get information about used extensions (unused).
    extensionData
};

///
/// DLL entry point which is called with index 0.. until it returns NULL.
///
LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    switch (index)
    {
        case 0:  return &descriptor;
        default: return NULL;
    }
}