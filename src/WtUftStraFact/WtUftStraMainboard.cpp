#include "WtUftStraMainboard.h"
#include "../Includes/IUftStraCtx.h"

#include "../Includes/WTSVariant.hpp"
#include "../Includes/WTSDataDef.hpp"
#include "../Includes/WTSContractInfo.hpp"
#include "../Share/TimeUtils.hpp"
#include "../Share/decimal.h"
#include "../Share/fmtlib.h"

extern const char *FACT_NAME;

WtUftStraMainboard::WtUftStraMainboard(const char *id)
	: UftStrategy(id), _last_tick(NULL), _last_entry_time(UINT64_MAX), _channel_ready(false), _last_calc_time(0), _lots(1), _cancel_cnt(0)
{
}

WtUftStraMainboard::~WtUftStraMainboard()
{
	if (_last_tick)
		_last_tick->release();
}

const char *WtUftStraMainboard::getName()
{
	return "UftMainboardStrategy";
}

const char *WtUftStraMainboard::getFactName()
{
	return FACT_NAME;
}

bool WtUftStraMainboard::init(WTSVariant *cfg)
{
	// 这里演示一下外部传入参数的获取
	_code = cfg->getCString("code");
	_secs = cfg->getUInt32("second");
	_freq = cfg->getUInt32("freq");
	_offset = cfg->getUInt32("offset");

	_lots = cfg->getDouble("lots");

	return true;
}

void WtUftStraMainboard::on_entrust(uint32_t localid, bool bSuccess, const char *message)
{
	if (!bSuccess)
	{
		auto it = _orders.find(localid);
		if (it != _orders.end())
			_orders.erase(it);
	}
}

void WtUftStraMainboard::on_init(IUftStraCtx *ctx)
{
	// ctx->watch_param("second", _secs);
	// ctx->watch_param("freq", _freq);
	// ctx->watch_param("offset", _offset);
	// ctx->watch_param("lots", _lots);
	// ctx->commit_param_watcher();

	// WTSKlineSlice* kline = ctx->stra_get_bars(_code.c_str(), "m1", 30);
	// if (kline)
	// 	kline->release();

	// ctx->stra_sub_ticks(_code.c_str());
	auto symbols = {"SSE.603628", "SZSE.000001"};
	for (auto symbol : symbols)
	{
		ctx->stra_sub_ticks(symbol);
		ctx->stra_sub_order_details(symbol);
		ctx->stra_sub_order_queues(symbol);
		ctx->stra_sub_transactions(symbol);
	}

	_ctx = ctx;
}

void WtUftStraMainboard::on_tick(IUftStraCtx *ctx, const char *code, WTSTickData *newTick)
{
	// if (_code.compare(code) != 0)
	// 	return;

	// if (!_orders.empty())
	// {
	// 	check_orders();
	// 	return;
	// }

	// if (!_channel_ready)
	// 	return;
	_ctx->stra_log_info(fmt::format("on_tick: {}", code).c_str());
}

void WtUftStraMainboard::check_orders()
{
	if (!_orders.empty() && _last_entry_time != UINT64_MAX)
	{
		uint64_t now = TimeUtils::makeTime(_ctx->stra_get_date(), _ctx->stra_get_time() * 100000 + _ctx->stra_get_secs());
		if (now - _last_entry_time >= _secs * 1000) // 如果超过一定时间没有成交完,则撤销
		{
			_mtx_ords.lock();
			for (auto localid : _orders)
			{
				_ctx->stra_cancel(localid);
				_cancel_cnt++;
				_ctx->stra_log_info(fmt::format("Order expired, cancelcnt updated to {}", _cancel_cnt).c_str());
			}
			_mtx_ords.unlock();
		}
	}
}
void WtUftStraMainboard::on_order_queue(IUftStraCtx *ctx, const char *stdCode, WTSOrdQueData *newOrdQue)
{
	_ctx->stra_log_info("on_order_queue");
}

void WtUftStraMainboard::on_transaction(IUftStraCtx *ctx, const char *code, WTSTransData *newTrans)
{
	_ctx->stra_log_info("on_transaction");
}
void WtUftStraMainboard::on_order_detail(IUftStraCtx *ctx, const char *stdCode, WTSOrdDtlData *newOrdDet)
{
	_ctx->stra_log_info("on_order_details");
}
void WtUftStraMainboard::on_bar(IUftStraCtx *ctx, const char *code, const char *period, uint32_t times, WTSBarStruct *newBar)
{
}

void WtUftStraMainboard::on_trade(IUftStraCtx *ctx, uint32_t localid, const char *stdCode, bool isLong, uint32_t offset, double qty, double price)
{
}

void WtUftStraMainboard::on_position(IUftStraCtx *ctx, const char *stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail)
{
	if (_code != stdCode)
		return;

	_prev = prevol;
	_ctx->stra_log_info(fmt::format("There are {} of {} before today", _prev, stdCode).c_str());
}

void WtUftStraMainboard::on_order(IUftStraCtx *ctx, uint32_t localid, const char *stdCode, bool isLong, uint32_t offset, double totalQty, double leftQty, double price, bool isCanceled)
{
	// 如果不是我发出去的订单,我就不管了
	auto it = _orders.find(localid);
	if (it == _orders.end())
		return;

	// 如果已撤销或者剩余数量为0,则清除掉原有的id记录
	if (isCanceled || leftQty == 0)
	{
		_mtx_ords.lock();
		_orders.erase(it);
		if (_cancel_cnt > 0)
		{
			_cancel_cnt--;
			_ctx->stra_log_info(fmt::format("cancelcnt -> {}", _cancel_cnt).c_str());
		}
		_mtx_ords.unlock();
	}
}

void WtUftStraMainboard::on_channel_ready(IUftStraCtx *ctx)
{
	double undone = _ctx->stra_get_undone(_code.c_str());
	if (!decimal::eq(undone, 0) && _orders.empty())
	{
		// 这说明有未完成单不在监控之中,先撤掉
		_ctx->stra_log_info(fmt::format("{}有不在管理中的未完成单 {} 手,全部撤销", _code, undone).c_str());

		OrderIDs ids = _ctx->stra_cancel_all(_code.c_str());
		for (auto localid : ids)
		{
			_orders.insert(localid);
		}
		_cancel_cnt += ids.size();

		_ctx->stra_log_info(fmt::format("cancelcnt -> {}", _cancel_cnt).c_str());
	}

	_channel_ready = true;
}

void WtUftStraMainboard::on_channel_lost(IUftStraCtx *ctx)
{
	_channel_ready = false;
}

void WtUftStraMainboard::on_params_updated()
{
	// ctx->watch_param("second", _secs);
	// ctx->watch_param("freq", _freq);
	// ctx->watch_param("offset", _offset);
	// ctx->watch_param("lots", _lots);

	_secs = _ctx->read_param("second", _secs);
	_freq = _ctx->read_param("freq", _freq);
	_offset = _ctx->read_param("offset", _offset);
	_lots = _ctx->read_param("lots", _lots);

	_ctx->stra_log_info(fmtutil::format("[{}] Params updated, second: {}, freq: {}, offset: {}, lots: {}", _id.c_str(), _secs, _freq, _offset, _lots));
}