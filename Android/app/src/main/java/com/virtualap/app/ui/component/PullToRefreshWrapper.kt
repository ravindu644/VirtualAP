package com.virtualap.app.ui.component

import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.size
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.pulltorefresh.PullToRefreshBox
import androidx.compose.material3.pulltorefresh.PullToRefreshDefaults
import androidx.compose.material3.pulltorefresh.rememberPullToRefreshState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.virtualap.app.ui.util.LoadingIndicator
import com.virtualap.app.ui.util.LoadingSize
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/**
 * Pull to refresh around a scrollable, with the app's own 24dp loader in the bubble, the
 * geometry Droidspaces uses; the library's stock indicator is larger. [onRefresh] runs in
 * a coroutine; the indicator stays up for at least 600 ms so a refresh that returns at
 * once still reads as feedback instead of a flicker.
 */
@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterial3ExpressiveApi::class)
@Composable
fun PullToRefreshWrapper(
    onRefresh: suspend () -> Unit,
    modifier: Modifier = Modifier,
    content: @Composable () -> Unit
) {
    var isRefreshing by remember { mutableStateOf(false) }
    val scope = rememberCoroutineScope()
    val state = rememberPullToRefreshState()

    PullToRefreshBox(
        isRefreshing = isRefreshing,
        onRefresh = {
            if (!isRefreshing) scope.launch {
                isRefreshing = true
                val start = System.currentTimeMillis()
                try {
                    onRefresh()
                    delay((600 - (System.currentTimeMillis() - start)).coerceAtLeast(0))
                } finally {
                    isRefreshing = false
                }
            }
        },
        state = state,
        modifier = modifier.fillMaxSize(),
        indicator = {
            PullToRefreshDefaults.IndicatorBox(
                state = state,
                isRefreshing = isRefreshing,
                modifier = Modifier.align(Alignment.TopCenter),
                containerColor = MaterialTheme.colorScheme.primaryContainer,
                elevation = 0.dp
            ) {
                if (isRefreshing) {
                    LoadingIndicator(size = LoadingSize.Medium, color = MaterialTheme.colorScheme.primary)
                } else {
                    androidx.compose.material3.LoadingIndicator(
                        progress = { state.distanceFraction },
                        modifier = Modifier.size(LoadingSize.Medium.size),
                        color = MaterialTheme.colorScheme.primary
                    )
                }
            }
        }
    ) {
        content()
    }
}
