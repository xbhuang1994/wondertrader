﻿#pragma once
#include <unordered_set>
#include <memory>
#include <thread>
#include <mutex>

#include "../Includes/UftStrategyDefs.h"
#include "../Share/SpinMutex.hpp"
#include <sw/redis++/redis++.h>

class WtUftStraMainboard : public UftStrategy
{
public:
	WtUftStraMainboard(const char* id);
	~WtUftStraMainboard();

private:
	void	check_orders();
	void init_redis(wtp::WTSVariant *cfg);

public:
	virtual const char* getName() override;

	virtual const char* getFactName() override;

    virtual bool init(WTSVariant *cfg) override;

    virtual void on_init(IUftStraCtx *ctx) override;

    virtual void on_tick(IUftStraCtx *ctx, const char *code, WTSTickData *newTick) override;

    virtual void on_bar(IUftStraCtx* ctx, const char* code, const char* period, uint32_t times, WTSBarStruct* newBar) override;

	virtual void on_trade(IUftStraCtx* ctx, uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double qty, double price) override;

	virtual void on_position(IUftStraCtx* ctx, const char* stdCode, bool isLong, double prevol, double preavail, double newvol, double newavail) override;

	virtual void on_order(IUftStraCtx* ctx, uint32_t localid, const char* stdCode, bool isLong, uint32_t offset, double totalQty, double leftQty, double price, bool isCanceled) override;

	virtual void on_channel_ready(IUftStraCtx* ctx) override;

	virtual void on_channel_lost(IUftStraCtx* ctx) override;

	virtual void on_entrust(uint32_t localid, bool bSuccess, const char* message) override;
	
	/*
	 *	委托队列推送
	 */
	virtual void on_order_queue(IUftStraCtx* ctx, const char* stdCode, WTSOrdQueData* newOrdQue) override;

	/*
	 *	逐笔委托推送
	 */
	virtual void on_order_detail (IUftStraCtx* ctx, const char* stdCode, WTSOrdDtlData* newOrdDtl) override;

	/*
	 *	逐笔成交推送
	 */
	virtual void on_transaction(IUftStraCtx* ctx, const char* stdCode, WTSTransData* newTrans) override;


	virtual void on_params_updated() override;

private:
	std::string    	_exchg;
	WTSTickData*	_last_tick;
	IUftStraCtx*	_ctx;
	double			_prev;

	typedef std::unordered_set<uint32_t> IDSet;
	IDSet			_orders;
	SpinMutex		_mtx_ords;

	uint64_t		_last_entry_time;

	bool			_channel_ready;
	uint32_t		_last_calc_time;
	uint32_t		_cancel_cnt;
	std::shared_ptr<sw::redis::Redis>	_redis;
};

