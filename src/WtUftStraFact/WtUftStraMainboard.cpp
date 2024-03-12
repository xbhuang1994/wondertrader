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
	: UftStrategy(id), _last_tick(NULL), _last_entry_time(UINT64_MAX), _channel_ready(false), _last_calc_time(0), _cancel_cnt(0)
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
	_exchg = cfg->getCString("exchg");
	auto redis_cfg = cfg->get("redis");
	initRedis(redis_cfg);

	auto low_factor_key = cfg->getCString("low_factor");
	auto low_factor_val = _redis->get(low_factor_key);
	if (low_factor_val)
	{
		
	// 	// std::string exchange_str = m_cfg["md"]["exchange"];
	// 	// json jsonArray = json::parse(*low_factor_str);
	// 	// for (auto &element : jsonArray)
	// 	// {
	// 	// 	if (exchange_str != element["exchange"])
	// 	// 	{
	// 	// 		continue;
	// 	// 	}
	// 	// 	std::string stock_id = element["security_id"];
	// 	// 	int sec = FactorLibrary::sec2num(stock_id.c_str(), 6);
	// 	// 	std::string yesterday_vol = element["yesterday_vol"];
	// 	// 	std::string total_market_cap = element["total_market_cap"];
	// 	// 	std::string circ_market_cap = element["circ_market_cap"];
	// 	// 	std::string circ_mv_z = element["circ_mv_z"];
	// 	// 	bool ban = element["ban"];
	// 	// 	std::string upper_limit_price = element["upper_limit_price"];
	// 	// 	std::string yesterday_close = element["close"];
	// 	// 	std::string f1217_153 = element["f1217_153"];
	// 	// 	std::string f1217_102 = element["f1217_102"];
	// 	// 	std::string f1217_51 = element["f1217_51"];
	// 	// 	// 初始化当前策略为 ban
	// 	// 	m_ban_stock_arr[sec] = ban;

	// 	// 	m_active_factor_library->upLowFactor(sec, std::stod(yesterday_vol), std::stod(total_market_cap), std::stod(circ_market_cap), std::stod(circ_mv_z), ban, std::stod(upper_limit_price), std::stod(yesterday_close), std::stod(f1217_153), std::stod(f1217_102), std::stod(f1217_51));
	// 	// }
	// 	return true;
	}
	// else
	// {
	// 	return false;
	// }

	return true;
}

void WtUftStraMainboard::initRedis(wtp::WTSVariant *redis_cfg)
{
	// using namespace sw::redis;
	using sw::redis::ConnectionOptions;
	using sw::redis::Redis;
	auto host = redis_cfg->getCString("host");
	auto port = redis_cfg->getUInt32("port");
	auto db = redis_cfg->getUInt32("db");
	auto password = redis_cfg->getCString("password");
	ConnectionOptions conn_options;
	conn_options.host = host;
	conn_options.port = port;
	conn_options.db = db;
	conn_options.password = password;
	_redis = std::make_shared<Redis>(conn_options);
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
	_ctx->stra_log_info(fmt::format("on_tick: {}", code).c_str());
}

void WtUftStraMainboard::check_orders()
{
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

	// _secs = _ctx->read_param("second", _secs);
	// _freq = _ctx->read_param("freq", _freq);
	// _offset = _ctx->read_param("offset", _offset);
	// _lots = _ctx->read_param("lots", _lots);

	// _ctx->stra_log_info(fmtutil::format("[{}] Params updated, second: {}, freq: {}, offset: {}, lots: {}", _id.c_str(), _secs, _freq, _offset, _lots));
}